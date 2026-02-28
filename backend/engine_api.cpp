#include "engine_api.h"

#include "engine.hpp"
#include "third_party/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <mutex>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define CC_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define CC_KEEPALIVE
#endif

using json = nlohmann::json;

namespace {

std::mutex g_api_mu;
commander::GameState g_state;
bool g_initialized = false;
std::string g_last_error;
std::string g_out;

std::string to_lower_ascii(std::string s) {
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

std::string normalize_side(const std::string& side) {
    std::string s = to_lower_ascii(side);
    return (s == "blue") ? "blue" : "red";
}

double browser_opening_time_limit_for_difficulty(const std::string& difficulty) {
    if (difficulty == "easy") return 0.15;
    if (difficulty == "hard") return 0.55;
    return 0.30;
}

json move_to_json(const commander::Move& m) {
    return json{{"pid", m.pid}, {"dc", m.dc}, {"dr", m.dr}};
}

json piece_to_json(const commander::PieceData& p) {
    return json{
        {"id", p.id},
        {"player", p.player},
        {"kind", p.kind},
        {"col", p.col},
        {"row", p.row},
        {"hero", p.hero},
        {"carrier_id", p.carrier_id}
    };
}

json state_to_json(const commander::SerializedState& s) {
    json pieces = json::array();
    for (const auto& p : s.pieces) pieces.push_back(piece_to_json(p));

    json legal = json::array();
    for (const auto& m : s.legal_moves) legal.push_back(move_to_json(m));

    return json{
        {"turn", s.turn},
        {"game_over", s.game_over},
        {"result", s.result},
        {"pieces", pieces},
        {"legal_moves", legal},
        {"has_last_move", s.has_last_move},
        {"last_move", move_to_json(s.last_move)},
        {"last_move_capture", s.last_move_capture},
        {"last_move_player", s.last_move_player},
        {"game_mode", s.game_mode},
        {"difficulty", s.difficulty},
        {"board", {{"cols", 11}, {"rows", 12}}}
    };
}

bool parse_move(const json& j, commander::Move& m) {
    if (!j.is_object()) return false;
    if (j.contains("pid") && j.contains("dc") && j.contains("dr")) {
        try {
            m.pid = j.at("pid").get<int>();
            m.dc = j.at("dc").get<int>();
            m.dr = j.at("dr").get<int>();
            return true;
        } catch (...) {
            return false;
        }
    }
    if (j.contains("piece_id") && j.contains("to_col") && j.contains("to_row")) {
        try {
            m.pid = j.at("piece_id").get<int>();
            m.dc = j.at("to_col").get<int>();
            m.dr = j.at("to_row").get<int>();
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool parse_move_string(const char* raw, commander::Move& out) {
    if (!raw) return false;
    std::string s(raw);
    if (s.empty()) return false;

    json j = json::parse(s, nullptr, false);
    if (!j.is_discarded()) {
        return parse_move(j, out);
    }

    int pid = -1;
    int dc = -1;
    int dr = -1;
    if (std::sscanf(s.c_str(), "%d,%d,%d", &pid, &dc, &dr) == 3) {
        out = commander::Move{pid, dc, dr};
        return true;
    }
    return false;
}

bool parse_piece(const json& j, commander::PieceData& out) {
    if (!j.is_object()) return false;
    try {
        out.id = j.at("id").get<int>();
        out.player = j.at("player").get<std::string>();
        out.kind = j.at("kind").get<std::string>();
        out.col = j.at("col").get<int>();
        out.row = j.at("row").get<int>();
        out.hero = j.value("hero", false);
        out.carrier_id = j.value("carrier_id", -1);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_state_json(const json& root, commander::GameState& out) {
    if (!root.is_object()) return false;
    if (!root.contains("pieces") || !root.at("pieces").is_array()) return false;

    commander::GameState tmp;
    tmp.current = root.value("turn", std::string("red"));
    if (tmp.current != "red" && tmp.current != "blue") tmp.current = "red";

    tmp.game_over = root.value("game_over", false);
    tmp.result = root.value("result", std::string());
    tmp.has_last_move = root.value("has_last_move", false);
    tmp.last_move_capture = root.value("last_move_capture", false);
    tmp.last_move_player = root.value("last_move_player", std::string("red"));
    if (tmp.last_move_player != "red" && tmp.last_move_player != "blue") tmp.last_move_player = "red";

    tmp.game_mode = root.value("game_mode", std::string("full"));
    tmp.difficulty = root.value("difficulty", std::string("medium"));

    if (root.contains("human_player")) {
        tmp.human_player = normalize_side(root.value("human_player", std::string("red")));
    } else {
        tmp.human_player = "red";
    }
    if (root.contains("bot_player")) {
        tmp.bot_player = normalize_side(root.value("bot_player", std::string("blue")));
    } else {
        tmp.bot_player = (tmp.human_player == "red") ? "blue" : "red";
    }

    if (root.contains("last_move") && root.at("last_move").is_object()) {
        commander::Move lm;
        if (parse_move(root.at("last_move"), lm)) {
            tmp.last_move = lm;
        }
    }

    tmp.pieces.clear();
    for (const auto& pj : root.at("pieces")) {
        commander::PieceData p;
        if (!parse_piece(pj, p)) return false;
        tmp.pieces.push_back(std::move(p));
    }

    // Position history is optional. If omitted, repetition checks restart from this position.
    tmp.position_history.clear();

    out = std::move(tmp);
    return true;
}

void set_error(const std::string& msg) {
    g_last_error = msg;
}

void clear_error() {
    g_last_error.clear();
}

const char* set_out_json(const json& body) {
    g_out = body.dump();
    return g_out.c_str();
}

const char* set_out_string(const std::string& body) {
    g_out = body;
    return g_out.c_str();
}

const char* current_state_json_locked() {
    commander::SerializedState s = commander::serialize_state(g_state);
    return set_out_json(state_to_json(s));
}

void ensure_initialized_locked() {
    if (g_initialized) return;
    g_state = commander::new_game("full", "medium");
    g_state.human_player = "red";
    g_state.bot_player = "blue";
    g_initialized = true;
}

} // namespace

extern "C" {

CC_KEEPALIVE int cc_init() {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();
    ensure_initialized_locked();
    return 1;
}

CC_KEEPALIVE int cc_new_game(const char* game_mode, const char* difficulty, const char* human_player) {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();

    std::string mode = game_mode ? game_mode : "full";
    std::string diff = difficulty ? difficulty : "medium";
    std::string human = normalize_side(human_player ? human_player : "red");

    try {
        g_state = commander::new_game(mode, diff);
        g_state.human_player = human;
        g_state.bot_player = (human == "red") ? "blue" : "red";

#if defined(__EMSCRIPTEN__)
        g_state.bot_time_limit = browser_opening_time_limit_for_difficulty(g_state.difficulty);
#endif

        if (g_state.current == g_state.bot_player) {
            commander::Move m = commander::bot_move(g_state);
            if (m.pid < 0) {
                set_error("bot could not find a legal move");
                return 0;
            }
        }

        g_initialized = true;
        return 1;
    } catch (const std::exception& ex) {
        set_error(ex.what());
        return 0;
    } catch (...) {
        set_error("failed to initialize game");
        return 0;
    }
}

CC_KEEPALIVE int cc_set_position(const char* state_json_or_fen) {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();

    if (!state_json_or_fen) {
        set_error("missing state string");
        return 0;
    }

    json root = json::parse(state_json_or_fen, nullptr, false);
    if (root.is_discarded()) {
        set_error("state parse failed; only JSON serialized state is supported");
        return 0;
    }

    commander::GameState parsed;
    if (!parse_state_json(root, parsed)) {
        set_error("invalid state JSON");
        return 0;
    }

    g_state = std::move(parsed);
    g_initialized = true;
    return 1;
}

CC_KEEPALIVE const char* cc_get_position() {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();
    ensure_initialized_locked();
    return current_state_json_locked();
}

CC_KEEPALIVE const char* cc_get_best_move(int time_ms, int depth) {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();
    ensure_initialized_locked();

    if (g_state.game_over) {
        return set_out_json(json::object());
    }

    commander::GameState probe = g_state;
    if (time_ms > 0) {
        probe.bot_time_limit = std::max(0.01, (double)time_ms / 1000.0);
    }
    if (depth > 0) {
        probe.bot_depth = std::max(1, depth);
    }

    commander::Move m = commander::bot_move(probe);
    if (m.pid < 0) {
        set_error("bot could not find a legal move");
        return set_out_json(json::object());
    }

    return set_out_json(move_to_json(m));
}

CC_KEEPALIVE int cc_apply_move(const char* move_uci_or_custom) {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();
    ensure_initialized_locked();

    commander::Move m;
    if (!parse_move_string(move_uci_or_custom, m)) {
        set_error("missing/invalid move payload");
        return 0;
    }

    commander::ActionStatus st = commander::apply_move(g_state, m);
    if (!st.ok) {
        set_error(st.error.empty() ? "illegal move" : st.error);
        return 0;
    }

    return 1;
}

CC_KEEPALIVE const char* cc_get_sprites_json() {
    std::lock_guard<std::mutex> lk(g_api_mu);
    clear_error();

    json sprites = json::object();
    for (const auto& kv : commander::piece_sprites()) {
        sprites[kv.first] = kv.second;
    }
    return set_out_json(json{{"sprites", sprites}});
}

CC_KEEPALIVE const char* cc_get_last_error() {
    std::lock_guard<std::mutex> lk(g_api_mu);
    return set_out_string(g_last_error);
}

} // extern "C"
