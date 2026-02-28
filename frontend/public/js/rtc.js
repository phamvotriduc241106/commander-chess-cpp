import {
  addDoc,
  collection,
  db,
  doc,
  onSnapshot,
  query,
  serverTimestamp
} from '/js/firebaseClient.js';

const DEFAULT_ICE_SERVERS = [
  { urls: ['stun:stun.l.google.com:19302', 'stun:stun1.l.google.com:19302'] }
];

function normalizeSignalPayload(payload) {
  if (payload == null) return payload;
  if (typeof payload === 'string' || typeof payload === 'number' || typeof payload === 'boolean') {
    return payload;
  }
  if (typeof payload.toJSON === 'function') {
    try {
      return payload.toJSON();
    } catch (_) {
      // continue to fallback conversion
    }
  }
  try {
    return JSON.parse(JSON.stringify(payload));
  } catch (_) {
    return payload;
  }
}

function safeCall(fn, ...args) {
  try {
    if (typeof fn === 'function') fn(...args);
  } catch (err) {
    console.warn('[Commander Chess] callback error', err);
  }
}

export function createMatchRtcTransport({
  matchId,
  role,
  onMessage = null,
  onState = null,
  iceServers = DEFAULT_ICE_SERVERS
} = {}) {
  if (!matchId) throw new Error('matchId is required for RTC transport');
  if (role !== 'host' && role !== 'guest') {
    throw new Error('role must be "host" or "guest"');
  }
  if (typeof RTCPeerConnection === 'undefined') {
    throw new Error('WebRTC is not supported in this browser');
  }

  const remoteRole = role === 'host' ? 'guest' : 'host';
  const signalsRef = collection(doc(db, 'matches', matchId), 'signals');

  let closed = false;
  let connected = false;
  let started = false;
  let peer = null;
  let channel = null;
  let signalUnsub = null;
  const seenSignalIds = new Set();
  const pendingRemoteIce = [];

  function emitState(state, detail = null) {
    safeCall(onState, { state, detail, role, matchId });
  }

  async function sendSignal(type, payload, to = remoteRole) {
    if (closed) return;
    await addDoc(signalsRef, {
      type,
      from: role,
      to,
      payload: normalizeSignalPayload(payload),
      createdAt: serverTimestamp()
    });
  }

  async function flushPendingIce() {
    if (!peer || !peer.remoteDescription || !pendingRemoteIce.length) return;
    while (pendingRemoteIce.length > 0) {
      const candidate = pendingRemoteIce.shift();
      try {
        await peer.addIceCandidate(candidate);
      } catch (err) {
        console.warn('[Commander Chess] addIceCandidate failed', err);
      }
    }
  }

  function bindChannel(dc) {
    channel = dc;
    channel.onopen = () => {
      connected = true;
      emitState('connected');
    };
    channel.onclose = () => {
      connected = false;
      emitState('disconnected');
    };
    channel.onerror = (ev) => {
      emitState('error', ev);
    };
    channel.onmessage = (ev) => {
      const raw = ev && typeof ev.data === 'string' ? ev.data : '';
      if (!raw) return;
      try {
        const msg = JSON.parse(raw);
        safeCall(onMessage, msg);
      } catch (err) {
        console.warn('[Commander Chess] invalid RTC payload', err);
      }
    };
  }

  function ensurePeer() {
    if (peer) return peer;
    peer = new RTCPeerConnection({ iceServers });

    peer.onicecandidate = (event) => {
      if (!event || !event.candidate) return;
      sendSignal('ice', event.candidate.toJSON ? event.candidate.toJSON() : event.candidate).catch((err) => {
        console.warn('[Commander Chess] signal ice failed', err);
      });
    };

    peer.onconnectionstatechange = () => {
      const state = peer.connectionState || 'unknown';
      if (state === 'connected') connected = true;
      if (state === 'failed' || state === 'disconnected' || state === 'closed') connected = false;
      emitState(state);
    };

    peer.ondatachannel = (event) => {
      if (!event || !event.channel) return;
      bindChannel(event.channel);
    };

    if (role === 'host') {
      const dc = peer.createDataChannel('moves', { ordered: true });
      bindChannel(dc);
    }

    return peer;
  }

  async function handleSignal(signal) {
    if (closed || !signal || signal.from === role) return;
    if (signal.to && signal.to !== role) return;

    const pc = ensurePeer();

    if (signal.type === 'offer' && role === 'guest') {
      if (pc.currentRemoteDescription) return;
      await pc.setRemoteDescription(new RTCSessionDescription(signal.payload));
      await flushPendingIce();
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      await sendSignal('answer', pc.localDescription, 'host');
      emitState('connecting');
      return;
    }

    if (signal.type === 'answer' && role === 'host') {
      if (pc.currentRemoteDescription) return;
      await pc.setRemoteDescription(new RTCSessionDescription(signal.payload));
      await flushPendingIce();
      emitState('connecting');
      return;
    }

    if (signal.type === 'ice') {
      const candidate = new RTCIceCandidate(signal.payload);
      if (!pc.remoteDescription) {
        pendingRemoteIce.push(candidate);
      } else {
        await pc.addIceCandidate(candidate);
      }
    }
  }

  async function startSignalingListener() {
    if (signalUnsub) return;
    signalUnsub = onSnapshot(query(signalsRef), (snap) => {
      snap.docChanges().forEach((change) => {
        if (change.type !== 'added') return;
        if (seenSignalIds.has(change.doc.id)) return;
        seenSignalIds.add(change.doc.id);
        handleSignal(change.doc.data()).catch((err) => {
          console.warn('[Commander Chess] signal handling failed', err);
          emitState('error', err);
        });
      });
    }, (err) => {
      emitState('error', err);
    });
  }

  async function connect() {
    if (started) return;
    started = true;
    emitState('signaling');
    const pc = ensurePeer();
    await startSignalingListener();

    if (role === 'host') {
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      await sendSignal('offer', pc.localDescription, 'guest');
    }
    emitState('connecting');
  }

  function isReady() {
    return !!(channel && channel.readyState === 'open' && connected);
  }

  async function waitUntilReady(timeoutMs = 15000) {
    if (isReady()) return true;

    const start = Date.now();
    while (!closed && Date.now() - start < timeoutMs) {
      if (isReady()) return true;
      await new Promise((resolve) => setTimeout(resolve, 120));
    }
    return isReady();
  }

  function send(payload) {
    if (!isReady()) return false;
    const text = JSON.stringify(payload || {});
    channel.send(text);
    return true;
  }

  function close() {
    closed = true;
    connected = false;
    if (signalUnsub) {
      signalUnsub();
      signalUnsub = null;
    }
    if (channel) {
      try { channel.close(); } catch (_) {}
      channel = null;
    }
    if (peer) {
      try { peer.close(); } catch (_) {}
      peer = null;
    }
    emitState('closed');
  }

  return {
    close,
    connect,
    isReady,
    matchId,
    role,
    send,
    waitUntilReady
  };
}
