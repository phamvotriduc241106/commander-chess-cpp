#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace commander {

struct Move {
    int pid = -1;
    int dc = -1;
    int dr = -1;
};

struct PieceData {
    int id = 0;
    std::string player;
    std::string kind;
    int col = 0;
    int row = 0;
    bool hero = false;
    int carrier_id = -1;
};

struct GameState {
    std::vector<PieceData> pieces;
    std::string current = "red";
    std::vector<uint64_t> position_history;

    bool game_over = false;
    std::string result;

    bool has_last_move = false;
    Move last_move{};
    bool last_move_capture = false;
    std::string last_move_player = "red";

    // Frontend defaults: human red vs bot blue.
    std::string human_player = "red";
    std::string bot_player = "blue";
    std::string game_mode = "full"; // full | marine | air | land
    std::string difficulty = "medium"; // easy | medium | hard

    int bot_depth = 4;
    double bot_time_limit = 0.20;
};

struct ActionStatus {
    bool ok = false;
    std::string error;
    bool game_over = false;
    std::string result;
};

struct SerializedState {
    std::string turn;
    bool game_over = false;
    std::string result;

    bool has_last_move = false;
    Move last_move{};
    bool last_move_capture = false;
    std::string last_move_player;
    std::string game_mode = "full";
    std::string difficulty = "medium";

    std::vector<PieceData> pieces;
    std::vector<Move> legal_moves;
};

GameState new_game(const std::string& game_mode = "full",
                   const std::string& difficulty = "medium");
ActionStatus apply_move(GameState& state, const Move& move);
Move bot_move(GameState& state); // returns {-1,-1,-1} on failure
SerializedState serialize_state(const GameState& state);

} // namespace commander
