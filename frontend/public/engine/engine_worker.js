let modulePromise = null;
let moduleInstance = null;
let api = null;

function parseJsonOrThrow(raw, label) {
  if (typeof raw !== 'string' || raw.length === 0) {
    throw new Error(`${label} returned empty payload`);
  }
  try {
    return JSON.parse(raw);
  } catch (err) {
    throw new Error(`${label} returned invalid JSON: ${err && err.message ? err.message : String(err)}`);
  }
}

async function loadFactory() {
  const mod = await import('/engine/commander_engine.js');
  return mod.default || mod.CommanderEngine || self.CommanderEngine;
}

async function ensureApi() {
  if (api) return api;
  if (!modulePromise) {
    modulePromise = loadFactory();
  }

  const factory = await modulePromise;
  if (typeof factory !== 'function') {
    throw new Error('Commander engine factory is unavailable');
  }

  moduleInstance = await factory();
  if (!moduleInstance || typeof moduleInstance.cwrap !== 'function') {
    throw new Error('Commander engine module failed to initialize');
  }

  api = {
    init: moduleInstance.cwrap('cc_init', 'number', []),
    newGame: moduleInstance.cwrap('cc_new_game', 'number', ['string', 'string', 'string']),
    setPosition: moduleInstance.cwrap('cc_set_position', 'number', ['string']),
    getPosition: moduleInstance.cwrap('cc_get_position', 'string', []),
    getBestMove: moduleInstance.cwrap('cc_get_best_move', 'string', ['number', 'number']),
    applyMove: moduleInstance.cwrap('cc_apply_move', 'number', ['string']),
    getSprites: moduleInstance.cwrap('cc_get_sprites_json', 'string', []),
    getLastError: moduleInstance.cwrap('cc_get_last_error', 'string', [])
  };

  return api;
}

function readLastError(apiRef, fallback) {
  try {
    const msg = apiRef.getLastError();
    if (typeof msg === 'string' && msg.trim()) return msg;
  } catch (_) {
    // fall through
  }
  return fallback;
}

async function handleCommand(cmd, payload) {
  const core = await ensureApi();

  if (cmd === 'init') {
    const ok = core.init();
    if (!ok) throw new Error(readLastError(core, 'cc_init failed'));
    return { ok: true };
  }

  if (cmd === 'newGame') {
    const gameMode = (payload && payload.gameMode) || 'full';
    const difficulty = (payload && payload.difficulty) || 'medium';
    const humanPlayer = (payload && payload.humanPlayer) || 'red';
    const ok = core.newGame(gameMode, difficulty, humanPlayer);
    if (!ok) throw new Error(readLastError(core, 'cc_new_game failed'));
    return parseJsonOrThrow(core.getPosition(), 'cc_get_position');
  }

  if (cmd === 'setPosition') {
    const state = payload && payload.state ? payload.state : {};
    const ok = core.setPosition(JSON.stringify(state));
    if (!ok) throw new Error(readLastError(core, 'cc_set_position failed'));
    return parseJsonOrThrow(core.getPosition(), 'cc_get_position');
  }

  if (cmd === 'getPosition') {
    return parseJsonOrThrow(core.getPosition(), 'cc_get_position');
  }

  if (cmd === 'applyMove') {
    const move = payload && payload.move ? payload.move : {};
    const ok = core.applyMove(JSON.stringify(move));
    if (!ok) throw new Error(readLastError(core, 'cc_apply_move failed'));
    return parseJsonOrThrow(core.getPosition(), 'cc_get_position');
  }

  if (cmd === 'getBestMove') {
    const timeMs = Number(payload && payload.timeMs) || 0;
    const depth = Number(payload && payload.depth) || 0;
    const move = parseJsonOrThrow(core.getBestMove(timeMs, depth), 'cc_get_best_move');
    return move;
  }

  if (cmd === 'getSprites') {
    return parseJsonOrThrow(core.getSprites(), 'cc_get_sprites_json');
  }

  throw new Error(`Unknown engine command: ${cmd}`);
}

self.onmessage = async (event) => {
  const msg = event && event.data ? event.data : {};
  const id = msg.id;
  const cmd = msg.cmd;
  const payload = msg.payload;

  try {
    const result = await handleCommand(cmd, payload);
    self.postMessage({ id, ok: true, result });
  } catch (err) {
    const message = err && err.message ? err.message : String(err);
    self.postMessage({ id, ok: false, error: message });
  }
};
