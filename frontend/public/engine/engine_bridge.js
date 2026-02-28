let worker = null;
let reqId = 1;
let initPromise = null;
let ready = false;
const pending = new Map();

function ensureWorker() {
  if (worker) return worker;
  worker = new Worker('/engine/engine_worker.js', { type: 'module' });

  worker.onmessage = (event) => {
    const msg = event && event.data ? event.data : {};
    const id = msg.id;
    if (!pending.has(id)) return;
    const p = pending.get(id);
    pending.delete(id);

    if (msg.ok) p.resolve(msg.result);
    else p.reject(new Error(msg.error || 'Engine worker request failed'));
  };

  worker.onerror = (event) => {
    const reason = event && event.message ? event.message : 'Engine worker crashed';
    pending.forEach((p) => p.reject(new Error(reason)));
    pending.clear();
    ready = false;
  };

  return worker;
}

function callWorker(cmd, payload = null) {
  const w = ensureWorker();
  const id = reqId++;

  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    try {
      w.postMessage({ id, cmd, payload: payload || {} });
    } catch (err) {
      pending.delete(id);
      reject(err);
    }
  });
}

export async function initEngine() {
  if (ready) return true;
  if (initPromise) return initPromise;

  initPromise = (async () => {
    await callWorker('init');
    ready = true;
    return true;
  })();

  try {
    return await initPromise;
  } catch (err) {
    initPromise = null;
    ready = false;
    throw err;
  }
}

export async function getSprites() {
  await initEngine();
  const result = await callWorker('getSprites');
  return result && result.sprites ? result.sprites : {};
}

export async function newGame({ gameMode = 'full', difficulty = 'medium', humanPlayer = 'red' } = {}) {
  await initEngine();
  return callWorker('newGame', { gameMode, difficulty, humanPlayer });
}

export async function setPosition(state) {
  await initEngine();
  return callWorker('setPosition', { state: state || {} });
}

export async function getPosition() {
  await initEngine();
  return callWorker('getPosition');
}

export async function applyMove(move) {
  await initEngine();
  return callWorker('applyMove', { move: move || {} });
}

export async function getBestMove({ timeMs = 0, depth = 0 } = {}) {
  await initEngine();
  return callWorker('getBestMove', { timeMs, depth });
}

const engineBridge = {
  initEngine,
  getSprites,
  newGame,
  setPosition,
  getPosition,
  applyMove,
  getBestMove
};

export default engineBridge;
