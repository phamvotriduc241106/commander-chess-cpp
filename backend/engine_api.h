#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialize internal engine state. Returns 1 on success.
int cc_init();

// Load a new game using existing mode/difficulty/human-side rules. Returns 1 on success.
int cc_new_game(const char* game_mode, const char* difficulty, const char* human_player);

// Set full game state from JSON (serialized state format). Returns 1 on success.
int cc_set_position(const char* state_json_or_fen);

// Get current game state as JSON string.
const char* cc_get_position();

// Compute best move without mutating game state.
// Returns JSON object string like {"pid":1,"dc":2,"dr":3}.
const char* cc_get_best_move(int time_ms, int depth);
const char* cc_cpu_pick_move(int time_ms, int depth);

// Apply move from JSON string ("pid/dc/dr"). Returns 1 on success.
int cc_apply_move(const char* move_uci_or_custom);

// Retrieve last API error string.
const char* cc_get_last_error();

#ifdef __cplusplus
}
#endif
