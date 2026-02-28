#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>

// Reuse exact engine/rule logic from the existing source file.
// Rename main to avoid symbol conflict when linking backend server.
#define main commander_chess_gui_main
#include "commander_chess.cpp"
#undef main

namespace commander {
namespace {

static std::once_flag g_engine_init_once;

static void ensure_engine_init() {
    std::call_once(g_engine_init_once, []() {
        init_zobrist();
        // Cloud Run instances can be memory-limited; force a compact TT.
        // The original allocator probes very large sizes first (2GB, 1GB, ...),
        // which can OOM-kill the container before fallback completes.
        for (size_t mb : {64ULL, 32ULL, 16ULL, 8ULL}) {
            try {
                tt_resize(mb);
                break;
            } catch (...) {
            }
        }
        if (!g_TT) tt_ensure_allocated();
        reset_search_tables();
    });
}

static PieceList to_core(const std::vector<PieceData>& src) {
    PieceList out;
    out.reserve(src.size());
    for (const auto& p : src) {
        out.push_back(Piece{p.id, p.player, p.kind, p.col, p.row, p.hero, p.carrier_id});
    }
    return out;
}

static std::vector<PieceData> from_core(const PieceList& src) {
    std::vector<PieceData> out;
    out.reserve(src.size());
    for (const auto& p : src) {
        out.push_back(PieceData{p.id, p.player, p.kind, p.col, p.row, p.hero, p.carrier_id});
    }
    return out;
}

static std::string to_lower_ascii(std::string s) {
    for (char& ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

static std::string normalize_mode(const std::string& game_mode) {
    std::string m = to_lower_ascii(game_mode);
    if (m == "marine") return "marine";
    if (m == "air") return "air";
    if (m == "land") return "land";
    return "full";
}

static std::string normalize_difficulty(const std::string& difficulty) {
    std::string d = to_lower_ascii(difficulty);
    if (d == "easy" || d == "beginner") return "easy";
    if (d == "hard" || d == "expert") return "hard";
    return "medium";
}

static void apply_mode_to_core(const std::string& game_mode) {
    const std::string m = normalize_mode(game_mode);
    if (m == "marine")      g_game_mode = GameMode::MARINE_BATTLE;
    else if (m == "air")    g_game_mode = GameMode::AIR_BATTLE;
    else if (m == "land")   g_game_mode = GameMode::LAND_BATTLE;
    else                    g_game_mode = GameMode::FULL_BATTLE;
}

static void apply_difficulty_to_core(const std::string& difficulty) {
    const std::string d = normalize_difficulty(difficulty);
#if defined(__EMSCRIPTEN__)
    // Keep browser builds single-threaded and deterministic.
    g_use_mcts = false;
#else
    g_use_mcts = (d == "hard");
#endif
}

static void apply_difficulty_to_state(GameState& state) {
    state.difficulty = normalize_difficulty(state.difficulty);
    if (state.difficulty == "easy") {
        state.bot_depth = 4;
        state.bot_time_limit = 2.5;
    } else if (state.difficulty == "hard") {
        state.bot_depth = 8;
        state.bot_time_limit = 8.0;
    } else {
        state.bot_depth = 6;
        state.bot_time_limit = 3.0;
    }
    apply_difficulty_to_core(state.difficulty);
}

static ActionStatus finalize_apply(GameState& state, PieceList& pieces_after, const std::string& mover) {
    ActionStatus st;
    st.ok = true;

    // Win check follows original game flow: check before switching side.
    std::string wm = check_win(pieces_after, mover);
    if (!wm.empty()) {
        state.pieces = from_core(pieces_after);
        state.game_over = true;
        state.result = wm;
        st.game_over = true;
        st.result = wm;
        return st;
    }

    state.current = opp(state.current);
    uint64_t h = zobrist_hash(pieces_after, state.current);
    push_position_history(state.position_history, h);
    if (is_threefold_repetition(state.position_history, h)) {
        state.pieces = from_core(pieces_after);
        state.game_over = true;
        state.result = "Draw — threefold repetition.";
        st.game_over = true;
        st.result = state.result;
        return st;
    }

    state.pieces = from_core(pieces_after);
    state.game_over = false;
    state.result.clear();
    st.game_over = false;
    return st;
}

} // namespace

GameState new_game(const std::string& game_mode, const std::string& difficulty) {
    ensure_engine_init();

    GameState out;
    out.game_mode = normalize_mode(game_mode);
    out.difficulty = normalize_difficulty(difficulty);
    apply_difficulty_to_state(out);
    apply_mode_to_core(out.game_mode);
    PieceList p = make_initial_pieces();
    out.pieces = from_core(p);
    out.current = "red";
    out.position_history.clear();
    push_position_history(out.position_history, zobrist_hash(p, out.current));
    out.game_over = false;
    out.result.clear();
    out.has_last_move = false;
    out.last_move = Move{};
    out.last_move_capture = false;
    out.last_move_player = "red";
    return out;
}

ActionStatus apply_move(GameState& state, const Move& move) {
    ensure_engine_init();
    apply_mode_to_core(state.game_mode);
    apply_difficulty_to_core(state.difficulty);

    ActionStatus st;
    st.ok = false;

    if (state.game_over) {
        st.error = "game is already over";
        st.game_over = true;
        st.result = state.result;
        return st;
    }

    PieceList pieces = to_core(state.pieces);
    Piece* piece = piece_by_id(pieces, move.pid);
    if (!piece) {
        st.error = "piece not found";
        return st;
    }
    if (piece->player != state.current) {
        st.error = "not this piece's turn";
        return st;
    }
    if (!has_legal_destination(*piece, pieces, move.dc, move.dr)) {
        st.error = "illegal move";
        return st;
    }

    Piece before = *piece;
    int enemy_before = 0;
    int own_before = 0;
    for (const auto& p : pieces) {
        if (p.player == state.current) own_before++;
        else enemy_before++;
    }

    PieceList after = ::apply_move(pieces, move.pid, move.dc, move.dr, state.current);

    int enemy_after = 0;
    int own_after = 0;
    for (const auto& p : after) {
        if (p.player == state.current) own_after++;
        else enemy_after++;
    }

    state.has_last_move = true;
    state.last_move = Move{before.id, move.dc, move.dr};
    state.last_move_capture = (enemy_after < enemy_before);
    state.last_move_player = state.current;

    return finalize_apply(state, after, state.last_move_player);
}

Move bot_move(GameState& state) {
    ensure_engine_init();
    apply_mode_to_core(state.game_mode);
    apply_difficulty_to_state(state);

    if (state.game_over) return Move{-1, -1, -1};

    PieceList pieces = to_core(state.pieces);
    reset_search_tables();
    tt_clear();
    g_game_rep_history = state.position_history;

    AIResult ai = cpu_pick_move(pieces, state.current, state.bot_depth, state.bot_time_limit);
    if (!ai.found) return Move{-1, -1, -1};

    Move m{ai.move.pid, ai.move.dc, ai.move.dr};
    ActionStatus st = apply_move(state, m);
    if (!st.ok) return Move{-1, -1, -1};
    return m;
}

SerializedState serialize_state(const GameState& state) {
    ensure_engine_init();
    apply_mode_to_core(state.game_mode);
    apply_difficulty_to_core(state.difficulty);

    SerializedState out;
    out.turn = state.current;
    out.game_over = state.game_over;
    out.result = state.result;
    out.has_last_move = state.has_last_move;
    out.last_move = state.last_move;
    out.last_move_capture = state.last_move_capture;
    out.last_move_player = state.last_move_player;
    out.game_mode = normalize_mode(state.game_mode);
    out.difficulty = normalize_difficulty(state.difficulty);
    out.pieces = state.pieces;

    if (state.game_over) return out;

    PieceList pieces = to_core(state.pieces);
    for (const auto& p : pieces) {
        if (p.player != state.current) continue;
        auto mvs = get_moves(p, pieces);
        for (const auto& m : mvs) {
            out.legal_moves.push_back(Move{p.id, m.first, m.second});
        }
    }
    return out;
}

std::unordered_map<std::string, std::string> piece_sprites() {
    ensure_engine_init();
    std::unordered_map<std::string, std::string> out;
    out.reserve(PIECE_B64.size());
    for (const auto& kv : PIECE_B64) out.emplace(kv.first, kv.second);
    return out;
}

} // namespace commander
