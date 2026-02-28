// True if `player` can win immediately in one move from `pieces`.
static bool has_immediate_winning_move(const PieceList& pieces, const std::string& player) {
    AllMoves moves = all_moves_for(pieces, player);
    for (auto& m : moves) {
        PieceList np = apply_move(pieces, m.pid, m.dc, m.dr, player);
        if (!check_win(np, player).empty()) return true;
    }
    return false;
}

static inline uint64_t zobrist_piece_key(const Piece& p);

static int quick_piece_unit_score(const Piece& p) {
    int val = piece_value_fast(p.kind);
    if (p.hero) val = (int)(val * 1.5f);
    // Fast positional component for incremental eval.
    val += get_pst(p.kind, p.player, p.col, p.row) * 2;
    return val;
}

static int quick_piece_score_cpu(const Piece& p, const std::string& cpu_player) {
    int s = quick_piece_unit_score(p);
    return p.player == cpu_player ? s : -s;
}

static int quick_eval_cpu(const PieceList& pieces, const std::string& cpu_player) {
    int v = 0;
    for (auto& p : pieces) v += quick_piece_score_cpu(p, cpu_player);
    return v;
}

struct AttackCache {
    bool valid = false;
    uint64_t key = 0; // state hash at which cache was built
    int counts[2][12][11]{};
    BB132 attacked_any[2];
    int attacked_square_count[2]{0, 0};
};

struct SearchState {
    PieceList pieces;
    std::string turn;
    uint64_t hash = 0;
    int quick_eval = 0; // from CPU perspective
    AttackCache atk;
    // Cached commander positions (indexed by player: 0=red, 1=blue), -1 if captured.
    int cmd_col[2] = {-1, -1};
    int cmd_row[2] = {-1, -1};
    // Cached navy counts per player.
    int navy_count[2] = {0, 0};

    void rebuild_caches() {
        cmd_col[0] = cmd_col[1] = -1;
        cmd_row[0] = cmd_row[1] = -1;
        navy_count[0] = navy_count[1] = 0;
        for (auto& p : pieces) {
            int pi = (p.player == "red") ? 0 : 1;
            if (p.kind == "C") { cmd_col[pi] = p.col; cmd_row[pi] = p.row; }
            if (p.kind == "N") navy_count[pi]++;
        }
    }
};

struct UndoMove {
    bool used_snapshot = false;
    PieceList snapshot_pieces;
    std::string turn_before;
    uint64_t hash_before = 0;
    int quick_eval_before = 0;
    int moved_idx_before = -1;
    int moved_idx_after = -1;
    int moved_id = -1;
    Piece moved_before{};
    bool moved_removed = false;
    bool had_capture = false;
    int captured_idx_before = -1;
    Piece captured_piece{};
};

static SearchState make_search_state(const PieceList& pieces, const std::string& turn,
                                     const std::string& cpu_player);

static int find_piece_idx_by_id(const PieceList& pieces, int pid) {
    for (int i = 0; i < (int)pieces.size(); i++) if (pieces[i].id == pid) return i;
    return -1;
}

static int find_piece_idx_at(const PieceList& pieces, int col, int row) {
    for (int i = 0; i < (int)pieces.size(); i++)
        if (pieces[i].carrier_id < 0 && pieces[i].col==col && pieces[i].row==row) return i;
    return -1;
}

static bool validate_state(const PieceList& pieces) {
    std::set<int> ids;
    std::set<std::pair<int,int>> occ;
    for (auto& p : pieces) {
        if (!on_board(p.col, p.row)) return false;
        if (!ids.insert(p.id).second) return false;
        if (p.carrier_id < 0 && !occ.insert({p.col, p.row}).second) return false;
    }
    for (auto& p : pieces) {
        if (p.carrier_id < 0) continue;
        if (p.carrier_id == p.id) return false;
        const Piece* c = piece_by_id_c(pieces, p.carrier_id);
        if (!c) return false;
        if (c->player != p.player) return false;
        if (!can_carry_kind(c->kind, p.kind)) return false;
        if (p.col != c->col || p.row != c->row) return false;
    }
    for (const auto& p : pieces) {
        if (!carrier_capacity_valid(pieces, p.id, p.kind)) return false;
    }
    return true;
}

// Sim-mode validator: catches corrupted states early and prints useful crash context.
static bool validate_state_for_sim(const PieceList& pieces,
                                   const std::string& last_mover,
                                   std::string* reason = nullptr) {
    std::set<int> ids;
    std::set<std::pair<int,int>> occ;
    int red_cmd = 0;
    int blue_cmd = 0;

    for (auto& p : pieces) {
        if (!on_board(p.col, p.row)) {
            if (reason) *reason = "piece out of bounds";
            return false;
        }
        if (!ids.insert(p.id).second) {
            if (reason) *reason = "duplicate piece id";
            return false;
        }
        if (p.carrier_id < 0 && !occ.insert({p.col, p.row}).second) {
            if (reason) *reason = "square occupied by multiple pieces";
            return false;
        }
        if (p.kind == "C") {
            if (p.player == "red") red_cmd++;
            else if (p.player == "blue") blue_cmd++;
        }
    }

    for (auto& p : pieces) {
        if (p.carrier_id < 0) continue;
        if (p.carrier_id == p.id) {
            if (reason) *reason = "piece carries itself";
            return false;
        }
        const Piece* c = piece_by_id_c(pieces, p.carrier_id);
        if (!c) {
            if (reason) *reason = "missing carrier piece";
            return false;
        }
        if (c->player != p.player) {
            if (reason) *reason = "cross-player carrying link";
            return false;
        }
        if (!can_carry_kind(c->kind, p.kind)) {
            if (reason) *reason = "illegal carrier/passenger pairing";
            return false;
        }
        if (p.col != c->col || p.row != c->row) {
            if (reason) *reason = "carried piece desynced from carrier";
            return false;
        }
    }
    for (const auto& p : pieces) {
        if (!carrier_capacity_valid(pieces, p.id, p.kind)) {
            if (reason) *reason = "carrier capacity exceeded";
            return false;
        }
    }

    if (red_cmd == 1 && blue_cmd == 1) return true;

    // Terminal commander-capture states are allowed if they are a legal win state.
    bool terminal_ok = (red_cmd == 0 && blue_cmd == 1) || (red_cmd == 1 && blue_cmd == 0);
    if (terminal_ok && !check_win(pieces, last_mover).empty()) return true;

    if (reason) {
        *reason = "invalid commander count (red=" + std::to_string(red_cmd) +
                  ", blue=" + std::to_string(blue_cmd) + ")";
    }
    return false;
}

static void build_attack_cache(SearchState& st) {
    if (st.atk.valid && st.atk.key == st.hash) return;
    memset(st.atk.counts, 0, sizeof(st.atk.counts));
    st.atk.attacked_any[0].clear();
    st.atk.attacked_any[1].clear();
    st.atk.attacked_square_count[0] = 0;
    st.atk.attacked_square_count[1] = 0;
    MoveGenContext ctx = build_movegen_context(st.pieces);
    for (auto& p : st.pieces) {
        int pl = (p.player=="red") ? 0 : 1;
        BB132 attacks = get_move_mask_bitboard(p, ctx);
        st.atk.attacked_any[pl].or_bits(attacks);
        while (true) {
            int sq = bb_pop_lsb(attacks);
            if (sq < 0) break;
            int c = sq_col(sq), r = sq_row(sq);
            st.atk.counts[pl][r][c]++;
        }
    }
    st.atk.attacked_square_count[0] = bb_popcount(st.atk.attacked_any[0]);
    st.atk.attacked_square_count[1] = bb_popcount(st.atk.attacked_any[1]);
    st.atk.valid = true;
    st.atk.key = st.hash;
}

static bool make_move_inplace(SearchState& st, const MoveTriple& m,
                              const std::string& cpu_player, UndoMove& u);

static void unmake_move_inplace(SearchState& st, const UndoMove& u);

// ── Transposition table ───────────────────────────────────────────────────
static const int TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2;

// ── Dynamic Transposition Table (heap-allocated, packed entries) ─────────
// Two-tier replacement:
//   slot 0: depth-preferred (replace only on equal/greater depth, or empty)
//   slot 1: always-replace (latest position)
static const int TT_BUCKET = 2;

// Packed TT entry: 20 bytes (was ~40). More entries per cache line = faster.
#pragma pack(push, 1)
struct TTEntry {
    uint64_t key   = 0;     // 8 bytes: full key
    int16_t  depth = 0;     // 2 bytes
    int16_t  val   = 0;     // 2 bytes (clamped ±32000)
    uint8_t  flag  = 0;     // 1 byte
    uint8_t  age   = 0;     // 1 byte
    int16_t  mv_pid = -1;   // 2 bytes: packed move
    int8_t   mv_dc  = -1;   // 1 byte
    int8_t   mv_dr  = -1;   // 1 byte  — Total: 20 bytes
};
#pragma pack(pop)

static inline MoveTriple tt_unpack_move(const TTEntry& e) {
    return {(int)e.mv_pid, (int)e.mv_dc, (int)e.mv_dr};
}
static inline void tt_pack_move(TTEntry& e, const MoveTriple& m) {
    e.mv_pid = (int16_t)m.pid;
    e.mv_dc  = (int8_t)m.dc;
    e.mv_dr  = (int8_t)m.dr;
}

struct TTCluster { TTEntry e[TT_BUCKET]; };

// Contiguous TT arena with configurable size
static size_t    g_tt_count = 0;      // number of clusters
static size_t    g_tt_mask  = 0;      // count - 1 (power of 2)
static TTCluster* g_TT      = nullptr;
static uint8_t   g_tt_age   = 0;
static void*     g_tt_arena = nullptr;
static size_t    g_tt_arena_bytes = 0;

static void tt_arena_release() {
    if (!g_tt_arena) return;
#if defined(_WIN32)
    VirtualFree(g_tt_arena, 0, MEM_RELEASE);
#elif defined(__unix__) || defined(__APPLE__)
    munmap(g_tt_arena, g_tt_arena_bytes);
#else
    std::free(g_tt_arena);
#endif
    g_tt_arena = nullptr;
    g_tt_arena_bytes = 0;
}

static void* tt_arena_alloc(size_t bytes) {
    if (bytes == 0) throw std::bad_alloc();
#if defined(_WIN32)
    void* p = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) throw std::bad_alloc();
    return p;
#elif defined(__unix__) || defined(__APPLE__)
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) throw std::bad_alloc();
#if defined(MADV_RANDOM)
    madvise(p, bytes, MADV_RANDOM);
#endif
#if defined(MADV_HUGEPAGE)
    madvise(p, bytes, MADV_HUGEPAGE);
#endif
    return p;
#else
    // Last-resort fallback for unsupported platforms.
    void* p = std::malloc(bytes);
    if (!p) throw std::bad_alloc();
    return p;
#endif
}

static void tt_resize(size_t size_mb) {
    tt_arena_release();
    g_TT = nullptr;
    g_tt_count = 0;
    g_tt_mask = 0;
    size_t bytes = size_mb * 1024ULL * 1024ULL;
    g_tt_count = bytes / sizeof(TTCluster);
    if (g_tt_count == 0) throw std::bad_alloc();
    // Round down to power of 2
    size_t pot = 1;
    while (pot * 2 <= g_tt_count) pot *= 2;
    g_tt_count = pot;
    g_tt_mask  = g_tt_count - 1;
    g_tt_arena_bytes = g_tt_count * sizeof(TTCluster);
    g_tt_arena = tt_arena_alloc(g_tt_arena_bytes);
    g_TT = reinterpret_cast<TTCluster*>(g_tt_arena);
    memset(g_TT, 0, g_tt_arena_bytes);
}

static void tt_ensure_allocated() {
    if (g_TT) return;
    for (size_t mb : {2048, 1024, 512, 256, 128, 64, 32, 8}) {
        try { tt_resize(mb); return; } catch (...) {}
    }
}

static void tt_clear() {
    if (g_TT && g_tt_count > 0)
        memset(g_TT, 0, g_tt_count * sizeof(TTCluster));
    g_tt_age = 0;
}

static inline void tt_prefetch(uint64_t h) {
#if defined(__GNUC__) || defined(__clang__)
    if (g_TT) __builtin_prefetch(&g_TT[h & g_tt_mask], 0, 1);
#endif
}

// Zobrist hashing (flattened piece-state x 132-square table)
static uint64_t g_ZobristTurn[2];

// Fast kind_index: direct char-based switch (no map lookup)
static int kind_index(const std::string& k) {
    if (k.empty()) return 0;
    switch (k[0]) {
        case 'C': return 0;
        case 'H': return 1;
        case 'I': return 2;  // In
        case 'M': return (k.size()>1 && k[1]=='s') ? 8 : 3;  // Ms vs M
        case 'T': return 4;
        case 'E': return 5;
        case 'A': if (k.size()>1) { if (k[1]=='a') return 7; if (k[1]=='f') return 9; }
                  return 6;  // A
        case 'N': return 10;
        default: return 0;
    }
}

static constexpr int ZK_KINDS   = 11;
static constexpr int ZK_PLAYERS = 2;
static constexpr int ZK_HERO    = 2;
static constexpr int ZK_CARRIED = 2;
static constexpr int ZK_STATES  = ZK_KINDS * ZK_PLAYERS * ZK_HERO * ZK_CARRIED; // 88 states
static constexpr int ZK_SQUARES = COLS * ROWS; // 132
static uint64_t g_ZK_piece_sq[ZK_STATES][ZK_SQUARES];

static inline int zobrist_piece_state_index(const Piece& p) {
    int ki = kind_index(p.kind);
    int pl = p.player=="red" ? 0 : 1;
    int hi = p.hero ? 1 : 0;
    int ci = (p.carrier_id >= 0) ? 1 : 0;
    return (((ki * ZK_PLAYERS + pl) * ZK_HERO + hi) * ZK_CARRIED + ci);
}

static uint64_t splitmix64_next(uint64_t& x) {
    x += 0x9E3779B97F4A7C15ULL;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void init_zobrist() {
    uint64_t seed = 0xC0FFEE1234567890ULL;
    for (int st = 0; st < ZK_STATES; st++)
        for (int sq = 0; sq < ZK_SQUARES; sq++)
            g_ZK_piece_sq[st][sq] = splitmix64_next(seed);
    g_ZobristTurn[0] = splitmix64_next(seed);
    g_ZobristTurn[1] = splitmix64_next(seed);
}

static uint64_t zobrist_hash(const PieceList& pieces, const std::string& turn) {
    uint64_t h = g_ZobristTurn[turn=="red" ? 0 : 1];
    for (auto& p : pieces) {
        if (p.row>=0 && p.row<12 && p.col>=0 && p.col<11) {
            int sq = sq_index(p.col, p.row);
            h ^= g_ZK_piece_sq[zobrist_piece_state_index(p)][sq];
        }
    }
    return h;
}

static inline uint64_t zobrist_piece_key(const Piece& p) {
    if (!on_board(p.col, p.row)) return 0;
    int sq = sq_index(p.col, p.row);
    return g_ZK_piece_sq[zobrist_piece_state_index(p)][sq];
}

static SearchState make_search_state(const PieceList& pieces, const std::string& turn,
                                     const std::string& cpu_player) {
    SearchState st;
    st.pieces = pieces;
    st.turn = turn;
    st.hash = zobrist_hash(st.pieces, st.turn);
    st.quick_eval = quick_eval_cpu(st.pieces, cpu_player);
    st.atk.valid = false;
    st.rebuild_caches();
    return st;
}

static bool make_move_inplace_snapshot(SearchState& st, const MoveTriple& m,
                                       const std::string& cpu_player, UndoMove& u) {
    u = UndoMove{};
    if (!on_board(m.dc, m.dr)) return false;
    int moved_idx = find_piece_idx_by_id(st.pieces, m.pid);
    if (moved_idx < 0) return false;
    if (st.pieces[moved_idx].player != st.turn) return false;
    if (!has_legal_destination(st.pieces[moved_idx], st.pieces, m.dc, m.dr)) return false;

    u.used_snapshot = true;
    u.snapshot_pieces = st.pieces;
    u.turn_before = st.turn;
    u.hash_before = st.hash;
    u.quick_eval_before = st.quick_eval;
    u.moved_id = m.pid;

    st.pieces = apply_move(st.pieces, m.pid, m.dc, m.dr, st.turn);
    st.turn = opp(st.turn);
    st.hash = zobrist_hash(st.pieces, st.turn);
    st.quick_eval = quick_eval_cpu(st.pieces, cpu_player);
    st.atk.valid = false;
    st.rebuild_caches();
    return true;
}

static bool make_move_inplace(SearchState& st, const MoveTriple& m,
                              const std::string& cpu_player, UndoMove& u) {
    u = UndoMove{};
    if (!on_board(m.dc, m.dr)) return false;
    int moved_idx = find_piece_idx_by_id(st.pieces, m.pid);
    if (moved_idx < 0) return false;
    if (st.pieces[moved_idx].player != st.turn) return false;

    int target_idx_probe = find_piece_idx_at(st.pieces, m.dc, m.dr);
    bool target_is_stackable_friendly = false;
    bool target_has_stack = false;
    if (target_idx_probe >= 0 && st.pieces[target_idx_probe].player == st.turn) {
        target_is_stackable_friendly = can_stack_together(st.pieces, st.pieces[moved_idx], st.pieces[target_idx_probe]);
    }
    if (target_idx_probe >= 0) {
        target_has_stack = piece_has_carried_children(st.pieces, st.pieces[target_idx_probe].id);
    }
    bool mover_is_carried = (st.pieces[moved_idx].carrier_id >= 0);
    bool mover_is_carrier = piece_has_carried_children(st.pieces, st.pieces[moved_idx].id);

    // Snapshot mode only when this move touches stack state.
    if (target_is_stackable_friendly || mover_is_carried || mover_is_carrier || target_has_stack) {
        return make_move_inplace_snapshot(st, m, cpu_player, u);
    }

    u.turn_before = st.turn;
    u.hash_before = st.hash;
    u.quick_eval_before = st.quick_eval;
    u.moved_id = m.pid;

    u.moved_idx_before = moved_idx;
    u.moved_before = st.pieces[moved_idx];

    auto remove_piece = [&](int idx) {
        const Piece oldp = st.pieces[idx];
        st.quick_eval -= quick_piece_score_cpu(oldp, cpu_player);
        st.hash ^= zobrist_piece_key(oldp);
        st.pieces.erase(st.pieces.begin() + idx);
    };
    auto update_piece = [&](int idx, const Piece& np) {
        const Piece oldp = st.pieces[idx];
        st.quick_eval -= quick_piece_score_cpu(oldp, cpu_player);
        st.hash ^= zobrist_piece_key(oldp);
        st.pieces[idx] = np;
        st.quick_eval += quick_piece_score_cpu(st.pieces[idx], cpu_player);
        st.hash ^= zobrist_piece_key(st.pieces[idx]);
    };
    auto apply_hero_promotions = [&]() {
        PieceList promoted = st.pieces;
        promote_heroes_from_checks(promoted);
        for (const auto& p2 : promoted) {
            if (!p2.hero) continue;
            int idx = find_piece_idx_by_id(st.pieces, p2.id);
            if (idx < 0 || st.pieces[idx].hero) continue;
            Piece np = st.pieces[idx];
            np.hero = true;
            update_piece(idx, np);
        }
    };

    st.hash ^= g_ZobristTurn[st.turn=="red" ? 0 : 1];

    int target_idx = find_piece_idx_at(st.pieces, m.dc, m.dr);
    bool navy_stays = false;
    if (target_idx >= 0) {
        const Piece& target = st.pieces[target_idx];
        if (target.player == st.turn) {
            st.hash = u.hash_before;
            st.quick_eval = u.quick_eval_before;
            return false;
        }
        navy_stays = (st.pieces[moved_idx].kind=="N" && !is_navigable(m.dc,m.dr))
                   || (st.pieces[moved_idx].kind=="T" && is_sea(m.dc,m.dr));
        u.had_capture = true;
        u.captured_idx_before = target_idx;
        u.captured_piece = target;
        remove_piece(target_idx);
        if (target_idx < moved_idx) moved_idx--;
    }

    moved_idx = find_piece_idx_by_id(st.pieces, m.pid);
    if (moved_idx < 0) {
        st.hash = u.hash_before;
        st.quick_eval = u.quick_eval_before;
        return false;
    }

    if (u.had_capture && st.pieces[moved_idx].kind=="Af" && !st.pieces[moved_idx].hero) {
        if (in_aa_range(st.pieces, m.dc, m.dr, st.turn)) {
            u.moved_removed = true;
            u.moved_idx_after = moved_idx;
            remove_piece(moved_idx);
            apply_hero_promotions();
            st.turn = opp(st.turn);
            st.hash ^= g_ZobristTurn[st.turn=="red" ? 0 : 1];
            st.atk.valid = false;
            st.rebuild_caches();
#ifdef DEBUG
            if (!validate_state(st.pieces)) { std::abort(); }
#endif
            return true;
        }
    }

    if (!navy_stays) {
        Piece np = st.pieces[moved_idx];
        np.col = m.dc;
        np.row = m.dr;
        update_piece(moved_idx, np);
    }

    moved_idx = find_piece_idx_by_id(st.pieces, m.pid);
    if (u.had_capture && moved_idx >= 0 &&
        st.pieces[moved_idx].kind == "Af" &&
        u.captured_piece.kind != "N" &&
        u.captured_piece.kind != "Af" && !navy_stays) {
        // Bombardment rule: unsafe AF land captures can return to origin.
        if (square_capturable_by_player(st.pieces, m.dc, m.dr, opp(st.turn))) {
            Piece np = st.pieces[moved_idx];
            np.col = u.moved_before.col;
            np.row = u.moved_before.row;
            update_piece(moved_idx, np);
        }
    }

    apply_hero_promotions();

    u.moved_idx_after = find_piece_idx_by_id(st.pieces, m.pid);
    st.turn = opp(st.turn);
    st.hash ^= g_ZobristTurn[st.turn=="red" ? 0 : 1];
    st.atk.valid = false;
    st.rebuild_caches();
#ifdef DEBUG
    if (!validate_state(st.pieces)) { std::abort(); }
#endif
    return true;
}

static void unmake_move_inplace(SearchState& st, const UndoMove& u) {
    if (u.used_snapshot) {
        st.turn = u.turn_before;
        st.hash = u.hash_before;
        st.quick_eval = u.quick_eval_before;
        st.pieces = u.snapshot_pieces;
        st.atk.valid = false;
        st.rebuild_caches();
#ifdef DEBUG
        if (!validate_state(st.pieces)) { std::abort(); }
#endif
        return;
    }

    st.turn = u.turn_before;
    st.hash = u.hash_before;
    st.quick_eval = u.quick_eval_before;

    if (u.moved_removed) {
        int idx = std::max(0, std::min((int)st.pieces.size(), u.moved_idx_after));
        st.pieces.insert(st.pieces.begin()+idx, u.moved_before);
    } else {
        int moved_idx = find_piece_idx_by_id(st.pieces, u.moved_id);
        if (moved_idx >= 0) st.pieces[moved_idx] = u.moved_before;
        else {
            int idx = std::max(0, std::min((int)st.pieces.size(), u.moved_idx_before));
            st.pieces.insert(st.pieces.begin()+idx, u.moved_before);
        }
    }
    if (u.had_capture) {
        int idx = std::max(0, std::min((int)st.pieces.size(), u.captured_idx_before));
        st.pieces.insert(st.pieces.begin()+idx, u.captured_piece);
    }
    st.atk.valid = false;
    st.rebuild_caches();
#ifdef DEBUG
    if (!validate_state(st.pieces)) { std::abort(); }
#endif
}

// ── Killer moves & History ─────────────────────────────────────────────────
static const int MAX_PLY = 32;

// Board dimensions for history indexing: [player][kind][col][row]
static const int H_PLAYERS = 2, H_KINDS = 11, H_COLS = 11, H_ROWS = 12;

// ── Per-thread search data for Lazy SMP ──────────────────────────────────
struct alignas(64) ThreadData {
    MoveTriple killers[MAX_PLY][2];
    bool killers_set[MAX_PLY][2];
    // Flat history: [player][kind_index][col][row] — O(1) access, memset-clearable
    int history[H_PLAYERS][H_KINDS][H_COLS][H_ROWS];
    // Flat continuation history: [prev_col][prev_row][kind_index][col][row]
    int16_t cont_history[H_COLS][H_ROWS][H_KINDS][H_COLS][H_ROWS];
    MoveTriple pv[MAX_PLY][MAX_PLY];
    int pv_len[MAX_PLY];
    MoveTriple counter[11][12];
    bool counter_set[11][12];
    int thread_id = 0;

    void reset() {
        memset(history, 0, sizeof(history));
        memset(cont_history, 0, sizeof(cont_history));
        for (int i = 0; i < MAX_PLY; i++) killers_set[i][0] = killers_set[i][1] = false;
        memset(pv_len, 0, sizeof(pv_len));
        memset(counter_set, 0, sizeof(counter_set));
    }
};

// Legacy globals — flat arrays for single-thread fallback & headless sim
static MoveTriple g_killers[MAX_PLY][2];
static bool g_killers_set[MAX_PLY][2];
static int g_history[H_PLAYERS][H_KINDS][H_COLS][H_ROWS];
static int16_t g_cont_history[H_COLS][H_ROWS][H_KINDS][H_COLS][H_ROWS];
static MoveTriple g_pv[MAX_PLY][MAX_PLY];
static int g_pv_len[MAX_PLY];
static MoveTriple g_counter[11][12];
static bool g_counter_set[11][12];

// Default thread data used by legacy single-thread path
static ThreadData g_default_td;

static void init_lmr_table();  // forward declaration

static void reset_search_tables() {
    tt_ensure_allocated();
    // Don't wipe TT — just age it so old entries get displaced naturally
    g_tt_age++;
    init_lmr_table();
    memset(g_history, 0, sizeof(g_history));
    memset(g_cont_history, 0, sizeof(g_cont_history));
    for (int i=0; i<MAX_PLY; i++) g_killers_set[i][0]=g_killers_set[i][1]=false;
    memset(g_pv_len, 0, sizeof(g_pv_len));
    memset(g_counter_set, 0, sizeof(g_counter_set));
    g_default_td.reset();
}

// ── TT probe / store ──────────────────────────────────────────────────────
static const TTEntry* tt_probe(uint64_t h) {
    if (!g_TT) return nullptr;
    const TTCluster& c = g_TT[h & g_tt_mask];
    const TTEntry* dp = &c.e[0];
    const TTEntry* ar = &c.e[1];
    bool dp_hit = (dp->key == h);
    bool ar_hit = (ar->key == h);
    if (dp_hit && ar_hit) {
        // Prefer same-generation entry, then deeper
        bool dp_current = (dp->age == g_tt_age);
        bool ar_current = (ar->age == g_tt_age);
        if (dp_current != ar_current) return dp_current ? dp : ar;
        return (dp->depth >= ar->depth) ? dp : ar;
    }
    if (dp_hit) return dp;
    if (ar_hit) return ar;
    return nullptr;
}

static void tt_store(uint64_t h, int depth, int flag, int val, MoveTriple best) {
    if (!g_TT) return;
    TTCluster& c = g_TT[h & g_tt_mask];
    auto write_entry = [&](TTEntry& e) {
        // Write key last so probes either see old entry or a mostly-complete new entry.
        e.key   = 0;
        e.depth = (int16_t)std::min(depth, (int)INT16_MAX);
        e.flag  = (uint8_t)flag;
        e.val   = (int16_t)std::max(-32000, std::min(32000, val));
        e.age   = g_tt_age;
        tt_pack_move(e, best);
        e.key   = h;
    };

    // Slot 0: depth-preferred, but prefer overwriting stale entries.
    TTEntry& depth_slot = c.e[0];
    bool slot0_stale = (depth_slot.age != g_tt_age);
    if (depth_slot.key == h) {
        if (depth >= depth_slot.depth || flag == TT_EXACT) write_entry(depth_slot);
    } else if (depth_slot.key == 0 || slot0_stale || depth >= depth_slot.depth) {
        write_entry(depth_slot);
    }

    // Slot 1: always replace.
    TTEntry& always_slot = c.e[1];
    write_entry(always_slot);
}

static void store_killer(const MoveTriple& m, int ply) {
    if (ply >= MAX_PLY) return;
    if (!g_killers_set[ply][0] || !(g_killers[ply][0].pid==m.pid && g_killers[ply][0].dc==m.dc && g_killers[ply][0].dr==m.dr)) {
        g_killers[ply][1] = g_killers[ply][0];
        g_killers_set[ply][1] = g_killers_set[ply][0];
        g_killers[ply][0] = m;
        g_killers_set[ply][0] = true;
    }
}

// ── ThreadData-aware versions for SMP ─────────────────────────────────────
static void td_store_killer(ThreadData& td, const MoveTriple& m, int ply) {
    if (ply >= MAX_PLY) return;
    if (!td.killers_set[ply][0] || !(td.killers[ply][0].pid==m.pid && td.killers[ply][0].dc==m.dc && td.killers[ply][0].dr==m.dr)) {
        td.killers[ply][1] = td.killers[ply][0];
        td.killers_set[ply][1] = td.killers_set[ply][0];
        td.killers[ply][0] = m;
        td.killers_set[ply][0] = true;
    }
}

static int td_history_score(const ThreadData& td, int pl, int ki, int dc, int dr) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return td.history[pl][ki][dc][dr];
}

static void td_update_history(ThreadData& td, int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = td.history[pl][ki][dc][dr];
    v += depth * depth;
    if (v > 32000) v = 32000;
}

static void td_penalise_history(ThreadData& td, int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = td.history[pl][ki][dc][dr];
    v -= depth * depth;
    if (v < -32000) v = -32000;
}

static int td_cont_history_score(const ThreadData& td, const MoveTriple* prev, int ki, int dc, int dr) {
    if (!prev || !on_board(prev->dc, prev->dr)) return 0;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return (int)td.cont_history[prev->dc][prev->dr][ki][dc][dr];
}

static void td_update_cont_history(ThreadData& td, const MoveTriple* prev, int ki, int dc, int dr, int depth) {
    if (!prev || !on_board(prev->dc, prev->dr)) return;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int v = (int)td.cont_history[prev->dc][prev->dr][ki][dc][dr] + depth*depth;
    td.cont_history[prev->dc][prev->dr][ki][dc][dr] = (int16_t)std::max(-32000, std::min(32000, v));
}

static int history_score(int pl, int ki, int dc, int dr) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return g_history[pl][ki][dc][dr];
}

static void update_history(int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = g_history[pl][ki][dc][dr];
    v += depth * depth;
    if (v > 32000) v = 32000;
}

static void penalise_history(int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = g_history[pl][ki][dc][dr];
    v -= depth * depth;
    if (v < -32000) v = -32000;
}

static int cont_history_score(const MoveTriple* prev, int ki, int dc, int dr) {
    if (!prev || !on_board(prev->dc, prev->dr)) return 0;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return (int)g_cont_history[prev->dc][prev->dr][ki][dc][dr];
}

static void update_cont_history(const MoveTriple* prev, int ki, int dc, int dr, int depth) {
    if (!prev || !on_board(prev->dc, prev->dr)) return;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int v = (int)g_cont_history[prev->dc][prev->dr][ki][dc][dr] + depth*depth;
    g_cont_history[prev->dc][prev->dr][ki][dc][dr] = (int16_t)std::max(-32000, std::min(32000, v));
}

// ── Static Exchange Evaluation (SEE) ─────────────────────────────────────
// Returns the material gain/loss of a capture sequence on (col,row).
// Positive = winning capture, negative = losing capture.
static int see(const PieceList& pieces, int col, int row,
               const std::string& attacker_player, int depth=0) {
    if (depth > 6) return 0;
    // Build context once for all pieces (avoids O(n²) rebuild per attacker).
    MoveGenContext ctx = build_movegen_context(const_cast<PieceList&>(pieces));
    // Find least-valuable attacker for attacker_player
    const Piece* best_atk = nullptr;
    int best_val = 999999;
    for (auto& p : pieces) {
        if (p.player != attacker_player) continue;
        auto mvs = get_moves_with_ctx(p, ctx);
        for (auto& m : mvs) {
            if (m.first==col && m.second==row) {
                int v = std::max(1, piece_value_fast(p.kind));
                if (v < best_val) { best_val=v; best_atk=&p; }
                break;
            }
        }
    }
    if (!best_atk) return 0;

    const Piece* target = piece_at_c(pieces, col, row);
    int gain = target ? piece_value_fast(target->kind) : 0;

    PieceList np = apply_move(pieces, best_atk->id, col, row, attacker_player);
    return gain - see(np, col, row, opp(attacker_player), depth+1);
}

// ── Move ordering ──────────────────────────────────────────────────────────
static AllMoves order_moves(const AllMoves& moves, const PieceList& pieces,
                             const std::string& player, int ply,
                             const MoveTriple* hash_move,
                             const MoveTriple* pv_move = nullptr,
                             const MoveTriple* prev_move = nullptr,
                             ThreadData* td = nullptr) {
    std::vector<std::pair<int,MoveTriple>> scored;
    scored.reserve(moves.size());
    const MoveTriple* counter_move = nullptr;
    if (prev_move && on_board(prev_move->dc, prev_move->dr)) {
        bool cs = td ? td->counter_set[prev_move->dc][prev_move->dr]
                      : g_counter_set[prev_move->dc][prev_move->dr];
        if (cs) counter_move = td ? &td->counter[prev_move->dc][prev_move->dr]
                                   : &g_counter[prev_move->dc][prev_move->dr];
    }
    int hist_pl = player_idx(player);
    if (hist_pl < 0) hist_pl = 0;
    for (auto& m : moves) {
        const Piece* piece = nullptr;
        for (auto& p : pieces) if (p.id==m.pid) { piece=&p; break; }
        if (!piece) continue;
        const Piece* target = piece_at_c(pieces, m.dc, m.dr);

        int score = 0;
        // 1. Hash move — search first always
        if (hash_move && same_move(*hash_move, m)) {
            score = 3000000;
        }
        // 1b. PV move from previous iteration
        else if (pv_move && same_move(*pv_move, m)) {
            score = 2500000;
        }
        // 2. Captures — scored by SEE (winning captures first, losing last)
        else if (target && target->player != player) {
            int victim_val = piece_value_fast(target->kind);
            int attacker_val = std::max(1, piece_value_fast(piece->kind));
            int mvv_lva = victim_val * 16 - attacker_val;
            int see_val = see(pieces, m.dc, m.dr, player);
            if (see_val >= 0)
                score = 1100000 + mvv_lva * 4 + see_val;   // winning / equal capture
            else
                score = 520000  + mvv_lva * 2 + see_val;   // losing capture (still above quiets)
        }
        // 2b. Counter move for previous move context
        else if (counter_move && same_move(*counter_move, m))
            score = 95000;
        // 3. Killer moves
        else if (ply<MAX_PLY && (td ? td->killers_set[ply][0] : g_killers_set[ply][0]) &&
                   (td ? td->killers[ply][0] : g_killers[ply][0]).pid==m.pid &&
                   (td ? td->killers[ply][0] : g_killers[ply][0]).dc==m.dc &&
                   (td ? td->killers[ply][0] : g_killers[ply][0]).dr==m.dr)
            score = 90000;
        else if (ply<MAX_PLY && (td ? td->killers_set[ply][1] : g_killers_set[ply][1]) &&
                   (td ? td->killers[ply][1] : g_killers[ply][1]).pid==m.pid &&
                   (td ? td->killers[ply][1] : g_killers[ply][1]).dc==m.dc &&
                   (td ? td->killers[ply][1] : g_killers[ply][1]).dr==m.dr)
            score = 89000;
        // 4. History heuristic for quiet moves
        else {
            int ki = kind_index(piece->kind);
            score = td ? td_history_score(*td, hist_pl, ki, m.dc, m.dr)
                       : history_score(hist_pl, ki, m.dc, m.dr);
            score += td ? td_cont_history_score(*td, prev_move, ki, m.dc, m.dr)
                        : cont_history_score(prev_move, ki, m.dc, m.dr);
        }

        scored.push_back({score, m});
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.first > b.first; });
    AllMoves result;
    result.reserve(scored.size());
    for (auto& s : scored) result.push_back(s.second);
    return result;
}

static int attackers_to_square(const PieceList& pieces, int col, int row,
                               const std::string& attacker_player,
                               const AttackCache* cache = nullptr) {
    if (cache && cache->valid) {
        int pl = attacker_player=="red" ? 0 : 1;
        return cache->counts[pl][row][col];
    }
    int attackers = 0;
    for (auto& p : pieces) {
        if (p.player != attacker_player) continue;
        auto mvs = get_moves(p, const_cast<PieceList&>(pieces));
        for (auto& m : mvs) {
            if (m.first == col && m.second == row) { attackers++; break; }
        }
    }
    return attackers;
}

static int count_kind_for(const PieceList& pieces, const std::string& player, const std::string& kind) {
    int n = 0;
    for (auto& p : pieces) if (p.player==player && p.kind==kind) n++;
    return n;
}

static bool side_has_only_pawn_militia_material(const PieceList& pieces, const std::string& player) {
    bool has_non_commander = false;
    for (const auto& p : pieces) {
        if (p.player != player) continue;
        if (p.kind == "C" || p.kind == "H") continue;
        has_non_commander = true;
        if (p.kind != "In" && p.kind != "M") return false;
    }
    return has_non_commander;
}

static int commander_attackers_cached(SearchState& st, const std::string& player) {
    int pi = (player == "red") ? 0 : 1;
    int cc = st.cmd_col[pi], cr = st.cmd_row[pi];
    if (cc < 0) return 0;
    build_attack_cache(st);
    return attackers_to_square(st.pieces, cc, cr, opp(player), &st.atk);
}

// ── Static evaluation (Phase-Interpolated) ────────────────────────────────
// Quadratic attacker penalty table: more attackers = exponentially worse
static const int CMD_ATTACKER_PENALTY[] = {0, 40, 120, 260, 450, 700, 1000};

enum class EvalBackendKind { CPU, WEBGPU };
static EvalBackendKind g_eval_backend = EvalBackendKind::CPU;
static std::atomic<bool> g_eval_webgpu_notice{false};

static const char* eval_backend_name(EvalBackendKind backend) {
    return backend == EvalBackendKind::WEBGPU ? "webgpu" : "cpu";
}

static bool eval_backend_webgpu_compiled() {
#if COMMANDER_HAS_WEBGPU_HEADER
    return true;
#else
    return false;
#endif
}

static std::string lower_ascii(std::string s) {
    for (char& ch : s)
        ch = (char)std::tolower((unsigned char)ch);
    return s;
}

static bool configure_eval_backend(const std::string& mode_raw, std::string* note = nullptr) {
    if (note) note->clear();
    std::string mode = lower_ascii(mode_raw);
    if (mode == "cpu") {
        g_eval_backend = EvalBackendKind::CPU;
        return true;
    }
    if (mode == "webgpu") {
        if (eval_backend_webgpu_compiled()) {
            g_eval_backend = EvalBackendKind::WEBGPU;
        } else {
            g_eval_backend = EvalBackendKind::CPU;
            if (note) *note = "WebGPU backend requested but build has no WebGPU/Dawn headers; using CPU evaluator.";
        }
        return true;
    }
    if (mode == "auto") {
        g_eval_backend = eval_backend_webgpu_compiled()
                         ? EvalBackendKind::WEBGPU
                         : EvalBackendKind::CPU;
        return true;
    }
    return false;
}

static EvalBackendKind active_eval_backend() {
    return g_eval_backend;
}

static int board_score_cpu_impl(const PieceList& pieces, const std::string& perspective,
                                const AttackCache* cache = nullptr,
                                const std::string* side_to_move = nullptr) {
    // ── Game Phase ───────────────────────────────────────────────────────
    int phase = compute_game_phase(pieces);

    // ── Constants (some phase-interpolated) ──────────────────────────────
    int THREAT_BONUS       = 350;     // was 260: stronger incentive to threaten commander
    int SPACE_ADV_WEIGHT   = (phase > 128) ? 4 : 6;  // was 2/4: push forward more
    int SPACE_CENTER_BONUS = (phase > 128) ? 12 : 18; // was 10/16
    int CMD_ATTACK_WEIGHT  = (phase > 128) ? 150 : 110; // was 90/70: much stronger attack reward
    int TEMPO_BONUS        = 20;       // was 18
    int CONTEMPT_BONUS     = 35;       // was 12: strongly prefer playing on over draws

    int score = 0;

    // ── Piece counts for strategic assessment ────────────────────────────
    int my_navy = 0, opp_navy = 0;
    int my_af = 0, opp_af = 0;
    int my_land = 0, opp_land = 0;
    int my_aa = 0, opp_aa = 0;
    int my_tank = 0, opp_tank = 0;
    int my_ms = 0, opp_ms = 0;
    int my_piece_count = 0, opp_piece_count = 0;

    const Piece* my_cmd  = nullptr;
    const Piece* opp_cmd = nullptr;

    for (auto& p : pieces) {
        bool mine = (p.player == perspective);
        if (p.kind=="C") { if (mine) my_cmd = &p; else opp_cmd = &p; continue; }
        if (p.kind=="H") continue;
        if (mine) my_piece_count++; else opp_piece_count++;
        if (p.kind=="N")  { if (mine) my_navy++; else opp_navy++; }
        if (p.kind=="Af") { if (mine) my_af++;   else opp_af++; }
        if (p.kind=="Aa") { if (mine) my_aa++;   else opp_aa++; }
        if (p.kind=="T")  { if (mine) my_tank++; else opp_tank++; }
        if (p.kind=="Ms") { if (mine) my_ms++;   else opp_ms++; }
        if (p.kind=="A" || p.kind=="T" || p.kind=="In") {
            if (mine) my_land++; else opp_land++;
        }
    }

    // ── Per-piece evaluation ─────────────────────────────────────────────
    for (auto& p : pieces) {
        if (p.kind == "H") continue; // HQ has no eval contribution
        bool mine = (p.player == perspective);
        int sign = mine ? 1 : -1;

        // Material
        int mat = piece_value_fast(p.kind);
        if (p.hero) mat = (mat * 3) / 2;  // heroes are 50% more valuable

        // Phase-interpolated PST
        int pst = get_pst_phased(p.kind, p.player, p.col, p.row, phase);

        // Threat bonus: piece can capture enemy Commander
        int threat = 0;
        if (p.kind != "H" && p.kind != "C" && !p.hero) {
            const Piece* oc = mine ? opp_cmd : my_cmd;
            if (oc && cache) {
                // Use attack cache: check if this side attacks the enemy commander's square
                int my_pl_idx = (p.player == "red") ? 0 : 1;
                if (cache->counts[my_pl_idx][oc->row][oc->col] > 0) {
                    threat = THREAT_BONUS;
                }
            } else if (oc) {
                auto mvs = get_moves(p, const_cast<PieceList&>(pieces));
                for (auto& m : mvs) {
                    if (m.first==oc->col && m.second==oc->row) {
                        threat = THREAT_BONUS; break;
                    }
                }
            }
        }

        // Hero proximity to enemy Commander
        int hero_bonus = 0;
        if (p.hero) {
            const Piece* ec = mine ? opp_cmd : my_cmd;
            if (ec) {
                int dist = std::abs(p.col - ec->col) + std::abs(p.row - ec->row);
                hero_bonus = std::max(0, 160 - dist * 18);
            }
        }

        // Space advance bonus (bigger in endgame)
        int space = 0;
        if (p.kind != "C" && p.kind != "H" && p.kind != "N") {
            int advance = (p.player=="red") ? p.row : (11 - p.row);
            space += advance * SPACE_ADV_WEIGHT;
            if (p.col >= 3 && p.col <= 7 && p.row >= 4 && p.row <= 7)
                space += SPACE_CENTER_BONUS;
        }

        // Hanging piece penalty: attacked but not defended
        int hanging = 0;
        if (cache && p.kind != "C") {
            int opp_pl = (p.player=="red") ? 1 : 0;
            int own_pl = 1 - opp_pl;
            int atk = cache->counts[opp_pl][p.row][p.col];
            int def = cache->counts[own_pl][p.row][p.col];
            if (atk > 0 && def == 0) {
                // Undefended and attacked: strong penalty proportional to piece value
                hanging = -(mat * 2 / 3);
            } else if (atk > def && mat > 200) {
                // Overloaded: penalty if attacked more than defended
                hanging = -(mat / 4);
            }
        }

        // Piece-type specific bonuses
        int special = 0;

        // Navy safety
        if (p.kind == "N") {
            int atk_n = attackers_to_square(pieces, p.col, p.row, opp(p.player), cache);
            int def_n = attackers_to_square(pieces, p.col, p.row, p.player, cache);
            special -= atk_n * 180;
            special += def_n * 70;
            if (atk_n > def_n) special -= (atk_n - def_n) * 140;
            if (is_sea(p.col, p.row)) special += 25;
        }

        // Air Force safety + Aa interaction
        if (p.kind == "Af") {
            int atk_f = attackers_to_square(pieces, p.col, p.row, opp(p.player), cache);
            int def_f = attackers_to_square(pieces, p.col, p.row, p.player, cache);
            special -= atk_f * 180;
            special += def_f * 65;
            if (atk_f > def_f) special -= (atk_f - def_f) * 300;
        }

        // Anti-air: bonus for covering friendly Af
        if (p.kind == "Aa") {
            for (auto& q : pieces) {
                if (q.player != p.player || q.kind != "Af") continue;
                int dist = std::abs(q.col - p.col) + std::abs(q.row - p.row);
                if (dist <= 3) special += 15;
                if (dist <= 1) special += 10;
            }
        }

        // Missile: bonus for being in range of high-value enemy targets
        if (p.kind == "Ms") {
            const Piece* ec = mine ? opp_cmd : my_cmd;
            if (ec) {
                int dist = std::abs(p.col - ec->col) + std::abs(p.row - ec->row);
                if (dist <= 4) special += 35;
                if (dist <= 2) special += 25;
            }
        }

        int total = mat + pst * 2 + threat + hero_bonus + space + hanging + special;
        score += sign * total;
    }

    // ── Commander Safety (phase-scaled) ──────────────────────────────────
    if (my_cmd) {
        int attackers = attackers_to_square(pieces, my_cmd->col, my_cmd->row, opp(perspective), cache);
        int n = std::min(attackers, 6);
        int cmd_penalty = CMD_ATTACKER_PENALTY[n];
        // Phase scale: safety matters much more in midgame
        cmd_penalty = (cmd_penalty * (128 + phase)) / 256;
        score -= cmd_penalty;

        // Shelter bonus: friendly pieces adjacent to our Commander
        int shelter = 0;
        for (int dc = -1; dc <= 1; dc++) for (int dr = -1; dr <= 1; dr++) {
            if (dc == 0 && dr == 0) continue;
            int c = my_cmd->col + dc, r = my_cmd->row + dr;
            if (!on_board(c, r)) continue;
            const Piece* occ = piece_at_c(pieces, c, r);
            if (occ && occ->player == perspective) shelter += 12;
        }
        score += (shelter * phase) / 256; // shelter matters more in midgame

        // Commander virtual mobility: count escape squares
        int escapes = 0;
        auto cmd_moves = get_moves(*my_cmd, const_cast<PieceList&>(pieces));
        for (auto& m : cmd_moves) {
            int opp_pl = (perspective == "red") ? 1 : 0;
            if (!cache || cache->counts[opp_pl][m.second][m.first] == 0) escapes++;
        }
        if (escapes <= 1) score -= 80;
        if (escapes == 0) score -= 150;
    }

    // ── Attack pressure on enemy Commander ───────────────────────────────
    if (opp_cmd) {
        int direct = attackers_to_square(pieces, opp_cmd->col, opp_cmd->row, perspective, cache);
        int defenders = attackers_to_square(pieces, opp_cmd->col, opp_cmd->row, opp(perspective), cache);
        score += direct * CMD_ATTACK_WEIGHT;
        score -= defenders * 18;

        // Ring control: attack squares around enemy Commander
        int ring_att = 0, ring_def = 0, ring_escape = 0;
        for (int dc = -1; dc <= 1; dc++) for (int dr = -1; dr <= 1; dr++) {
            if (dc == 0 && dr == 0) continue;
            int c = opp_cmd->col + dc, r = opp_cmd->row + dr;
            if (!on_board(c, r)) continue;
            ring_att += attackers_to_square(pieces, c, r, perspective, cache);
            ring_def += attackers_to_square(pieces, c, r, opp(perspective), cache);
            const Piece* occ = piece_at_c(pieces, c, r);
            if (!occ || occ->player != opp(perspective)) ring_escape++;
        }
        score += (ring_att - ring_def) * 18;  // was 12: stronger ring control incentive
        score -= ring_escape * 12; // was 8: more reward for trapping enemy commander
    }

    // ── Approximate mobility from attack cache ──────────────────────────
    // Much faster than generating all legal moves (the old method).
    if (cache) {
        int my_pl  = (perspective == "red") ? 0 : 1;
        int opp_pl = 1 - my_pl;
        int my_squares = cache->attacked_square_count[my_pl];
        int opp_squares = cache->attacked_square_count[opp_pl];
        int mob_weight = (phase > 128) ? 3 : 5; // mobility matters more in endgame
        score += (my_squares - opp_squares) * mob_weight;
    }

    // ── Piece pair bonuses ───────────────────────────────────────────────
    // Having both of a pair provides synergy
    if (my_navy == 2)  score += 100;
    if (opp_navy == 2) score -= 100;
    if (my_af == 2)    score += 80;
    if (opp_af == 2)   score -= 80;
    if (my_tank == 2)  score += 50;
    if (opp_tank == 2) score -= 50;

    // ── Structural bonuses ───────────────────────────────────────────────
    for (auto& p : pieces) {
        int bonus = 0;
        if      (p.kind == "Aa") bonus = 14;
        else if (p.kind == "Ms") bonus = 18;
        else if (p.kind == "N")  bonus = 10;
        score += (p.player == perspective ? 1 : -1) * bonus;
    }

    // ── Strategic objective pressure (navy — smoothed lookup table) ─────
    // Navy count → strategic value: {0: -2000, 1: +600, 2: +2500}
    // This avoids the huge non-linear jump between 1 and 2 navies.
    static const int NAVY_STRAT[] = {-2000, 600, 2500};
    score += NAVY_STRAT[std::min(my_navy, 2)] - NAVY_STRAT[std::min(opp_navy, 2)];

    score += (my_af - opp_af) * 700;
    if (my_af == 1)  score -= 450;
    if (opp_af == 1) score += 450;
    if (my_af == 0)  score -= 1200;
    if (opp_af == 0) score += 1200;

    score += (my_land - opp_land) * 220;
    if (my_land <= 2)  score -= 350;
    if (opp_land <= 2) score += 350;

    // ── Tempo & contempt ─────────────────────────────────────────────────
    if (side_to_move)
        score += (*side_to_move == perspective) ? TEMPO_BONUS : -TEMPO_BONUS;

    // Material advantage conversion: when ahead, fewer opponent pieces = better.
    // This encourages trading when ahead (amplifies advantage).
    int mat_diff = my_piece_count - opp_piece_count;
    if (mat_diff > 0) {
        // Reward: bonus scales with our advantage AND how few pieces opponent has
        int trade_bonus = mat_diff * (20 - opp_piece_count) * 3;
        if (trade_bonus > 0) score += trade_bonus;
    } else if (mat_diff < 0) {
        int trade_bonus = (-mat_diff) * (20 - my_piece_count) * 3;
        if (trade_bonus > 0) score -= trade_bonus;
    }

    // Contempt: always prefer playing on rather than accepting a draw.
    // Applied unconditionally (not just in close positions) to fight draw epidemic.
    score += CONTEMPT_BONUS;

    return score;
}

struct EvalBatchRequest {
    const PieceList* pieces;
    const std::string* perspective;
    const AttackCache* cache;
    const std::string* side_to_move;
};

static std::vector<int> board_score_batch_cpu_impl(const std::vector<EvalBatchRequest>& batch) {
    std::vector<int> out;
    out.reserve(batch.size());
    for (const auto& req : batch) {
        if (!req.pieces || !req.perspective) {
            out.push_back(0);
            continue;
        }
        out.push_back(board_score_cpu_impl(*req.pieces, *req.perspective, req.cache, req.side_to_move));
    }
    return out;
}

static std::vector<int> board_score_batch_webgpu_impl(const std::vector<EvalBatchRequest>& batch) {
    // Placeholder for true GPU batched eval: keep API stable and route to CPU now.
    bool expected = false;
    if (g_eval_webgpu_notice.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        std::cerr << "[eval] webgpu backend selected; batched evaluator path enabled, CPU fallback active.\n";
    }
    return board_score_batch_cpu_impl(batch);
}

static std::vector<int> board_score_batch(const std::vector<EvalBatchRequest>& batch) {
    if (active_eval_backend() == EvalBackendKind::WEBGPU)
        return board_score_batch_webgpu_impl(batch);
    return board_score_batch_cpu_impl(batch);
}

static int board_score_webgpu_impl(const PieceList& pieces, const std::string& perspective,
                                   const AttackCache* cache = nullptr,
                                   const std::string* side_to_move = nullptr) {
    EvalBatchRequest req{&pieces, &perspective, cache, side_to_move};
    auto out = board_score_batch_webgpu_impl({req});
    return out.empty() ? 0 : out[0];
}

static int board_score(const PieceList& pieces, const std::string& perspective,
                       const AttackCache* cache = nullptr,
                       const std::string* side_to_move = nullptr) {
    if (active_eval_backend() == EvalBackendKind::WEBGPU)
        return board_score_webgpu_impl(pieces, perspective, cache, side_to_move);
    return board_score_cpu_impl(pieces, perspective, cache, side_to_move);
}

// ── Quiescence ────────────────────────────────────────────────────────────
static std::atomic<uint64_t> g_nodes{0};  // global node counter for NPS / time mgmt
static const int Q_LIMIT   = 6;   // raised from 4 for deeper tactical vision
static const int DELTA_MARGIN = 200; // delta pruning margin

static int quiesce(SearchState& st, int alpha, int beta,
                   const std::string& perspective, const std::string& cpu_player,
                   int q_depth=0) {
    g_nodes.fetch_add(1, std::memory_order_relaxed);
    int stand = (perspective == cpu_player) ? st.quick_eval : -st.quick_eval;
    if (q_depth == 0) {
        build_attack_cache(st);
        int precise = board_score(st.pieces, perspective, &st.atk, &perspective);
        stand = (stand * 2 + precise) / 3;
    }
    if (stand >= beta) return beta;
    // Delta pruning: skip positions where even the best capture can't improve alpha
    if (stand < alpha - DELTA_MARGIN - 800) return alpha;
    if (alpha < stand) alpha = stand;
    if (q_depth >= Q_LIMIT) return alpha;

    // Order captures by SEE score before iterating (stack-allocated, no heap)
    struct CapMove { int pid, dc, dr, see_val; };
    CapMove caps[96];  // max captures well below 96 on this board
    int ncaps = 0;
    for (auto& p : st.pieces) {
        if (p.player != perspective) continue;
        auto mvs = get_moves(p, st.pieces);
        for (auto& m : mvs) {
            const Piece* t = piece_at_c(st.pieces, m.first, m.second);
            if (!t || t->player == perspective) continue;
            int sv = see(st.pieces, m.first, m.second, perspective);
            if (ncaps < 96) caps[ncaps++] = {p.id, m.first, m.second, sv};
        }
    }
    // Insertion sort (fast for small N, no allocations)
    for (int i = 1; i < ncaps; i++) {
        CapMove key = caps[i];
        int j = i - 1;
        while (j >= 0 && caps[j].see_val < key.see_val) {
            caps[j+1] = caps[j];
            j--;
        }
        caps[j+1] = key;
    }

    for (int ci = 0; ci < ncaps; ci++) {
        auto& c = caps[ci];
        // SEE pruning: skip clearly losing captures in quiescence
        if (c.see_val < 0 && q_depth >= 1) continue;
        // Delta pruning: skip captures that can't raise alpha even optimistically
        if (c.see_val + stand + DELTA_MARGIN < alpha) continue;
        UndoMove u;
        if (!make_move_inplace(st, {c.pid, c.dc, c.dr}, cpu_player, u)) continue;
        int s = -quiesce(st, -beta, -alpha, opp(perspective), cpu_player, q_depth+1);
        unmake_move_inplace(st, u);
        if (s >= beta) return beta;
        if (s > alpha) alpha = s;
    }
    return alpha;
}

// ── LMR Reduction Table (logarithmic) ────────────────────────────────────
static int g_lmr_table[64][64];
static bool g_lmr_init = false;

static void init_lmr_table() {
    if (g_lmr_init) return;
    for (int d = 0; d < 64; d++) {
        for (int m = 0; m < 64; m++) {
            if (d == 0 || m == 0) { g_lmr_table[d][m] = 0; continue; }
            g_lmr_table[d][m] = (int)(0.75 + std::log(d) * std::log(m) / 2.25);
            if (g_lmr_table[d][m] < 0) g_lmr_table[d][m] = 0;
        }
    }
    g_lmr_init = true;
}

static int lmr_reduction(int depth, int move_index) {
    int d = std::min(depth, 63);
    int m = std::min(move_index, 63);
    return g_lmr_table[d][m];
}

// ── Alpha-Beta with PVS + LMR + NMP + SEE Pruning + Improving ───────────
static thread_local std::chrono::steady_clock::time_point g_deadline;
static thread_local const std::atomic<bool>* g_stop_flag = nullptr;

// Throttled time check: only syscall every 4096 nodes to reduce overhead.
static thread_local uint64_t g_time_check_counter = 0;
static thread_local bool g_time_up_cache = false;

static bool time_up() {
    if (g_time_up_cache) return true;
    if (++g_time_check_counter & 255) return false;  // check every 256 nodes
    bool up = std::chrono::steady_clock::now() > g_deadline ||
              (g_stop_flag && g_stop_flag->load(std::memory_order_relaxed));
    if (up) g_time_up_cache = true;
    return up;
}

static void reset_time_state() {
    g_time_check_counter = 0;
    g_time_up_cache = false;
}

static thread_local std::vector<uint64_t> g_search_hash_path;

// Game-level repetition history: seeded into g_search_hash_path before search
// so the engine avoids moves that create threefold repetition with prior positions.
static thread_local std::vector<uint64_t> g_game_rep_history;

struct SearchPathGuard {
    bool active = false;
    explicit SearchPathGuard(uint64_t h) {
        g_search_hash_path.push_back(h);
        active = true;
    }
    ~SearchPathGuard() {
        if (active && !g_search_hash_path.empty()) g_search_hash_path.pop_back();
    }
};

static bool path_is_threefold(uint64_t h) {
    int cnt = 0;
    for (auto it = g_search_hash_path.rbegin(); it != g_search_hash_path.rend(); ++it) {
        if (*it == h && ++cnt >= 3) return true;
    }
    return false;
}

static int alphabeta(SearchState& st, int depth, int alpha, int beta,
                     bool is_max, const std::string& cpu_player, int ply,
                     bool null_ok=true, const MoveTriple* prev_move=nullptr,
                     ThreadData* td=nullptr) {
    SearchPathGuard path_guard(st.hash);
    if (path_is_threefold(st.hash)) return 0;

    (void)is_max;
    g_nodes.fetch_add(1, std::memory_order_relaxed);
    const bool node_is_max = (st.turn == cpu_player);
    if (ply < MAX_PLY) { if (td) td->pv_len[ply] = ply; else g_pv_len[ply] = ply; }

    // Hard safety guard against runaway recursion in extended lines.
    if (ply >= MAX_PLY) {
        if (node_is_max) return quiesce(st, alpha, beta, cpu_player, cpu_player);
        return -quiesce(st, -beta, -alpha, opp(cpu_player), cpu_player);
    }

    int orig_alpha = alpha;
    int orig_beta  = beta;
    bool pv_node   = (beta - alpha > 1);

    // ── Terminal: win check ───────────────────────────────────────────────
    std::string last_mover = opp(st.turn);
    std::string win = check_win(st.pieces, last_mover);
    if (!win.empty()) {
        int base = 40000 + depth*100;
        return (last_mover == cpu_player) ? base : -base;
    }
    if (depth == 0) {
        // Quiescence is negamax-style from side-to-move perspective.
        if (node_is_max) return quiesce(st, alpha, beta, cpu_player, cpu_player);
        return -quiesce(st, -beta, -alpha, opp(cpu_player), cpu_player);
    }

    // ── TT lookup ─────────────────────────────────────────────────────────
    uint64_t h = st.hash;
    const MoveTriple* hash_move_ptr = nullptr;
    MoveTriple hash_move_buf{};
    const TTEntry* tte = tt_probe(h);
    if (tte && tte->depth >= depth && !pv_node) {
        if      (tte->flag==TT_EXACT) return tte->val;
        else if (tte->flag==TT_LOWER && tte->val>alpha) alpha=tte->val;
        else if (tte->flag==TT_UPPER && tte->val<beta)  beta=tte->val;
        if (alpha >= beta) return tte->val;
    }
    if (tte) { hash_move_buf = tt_unpack_move(*tte); hash_move_ptr = &hash_move_buf; }

    // ── Internal Iterative Reduction (IIR) ───────────────────────────────
    // When we have no hash move, reduce depth by 1 instead of expensive IID.
    // The search at reduced depth will populate the TT for future iterations.
    if (!hash_move_ptr && depth >= 6 && !pv_node) {
        depth--;
    }

    int static_eval = st.quick_eval;

    // ── "Improving" heuristic: is our eval better than 2 plies ago? ──────
    // FIX: reset at root so stale values from the previous search can't
    // incorrectly influence the improving flag in the new search.
    static thread_local int ply_eval[MAX_PLY + 4];
    if (ply == 0) std::fill(std::begin(ply_eval), std::end(ply_eval), 0);
    int eval_from_perspective = node_is_max ? static_eval : -static_eval;
    if (ply < MAX_PLY) ply_eval[ply] = eval_from_perspective;
    bool improving = (ply >= 2 && eval_from_perspective > ply_eval[ply - 2]);

    // ── Pruning safety check ─────────────────────────────────────────────
    // Disable aggressive pruning when either commander is under attack at any depth.
    bool pruning_safe = true;
    {
        int cpu_cmd_atk = commander_attackers_cached(st, cpu_player);
        int opp_cmd_atk = commander_attackers_cached(st, opp(cpu_player));
        pruning_safe = (cpu_cmd_atk == 0 && opp_cmd_atk == 0);
    }

    // ── Reverse Futility Pruning (extended to depth 3) ───────────────────
    if (pruning_safe && !pv_node && depth <= 3) {
        int rfp_margin = (improving ? 150 : 200) * depth + 100;
        if (node_is_max && static_eval - rfp_margin >= beta) return static_eval;
        if (!node_is_max && static_eval + rfp_margin <= alpha) return static_eval;
    }

    // ── Razoring (extended to depth 1-3) ─────────────────────────────────
    if (pruning_safe && !pv_node && depth <= 3) {
        int razor_margin = 200 + 180 * (depth - 1);
        if (node_is_max && static_eval + razor_margin <= alpha) {
            if (depth <= 1) return quiesce(st, alpha, beta, cpu_player, cpu_player);
            int razor_val = quiesce(st, alpha, beta, cpu_player, cpu_player);
            if (razor_val <= alpha) return razor_val;
        }
        if (!node_is_max && static_eval - razor_margin >= beta) {
            if (depth <= 1) return -quiesce(st, -beta, -alpha, opp(cpu_player), cpu_player);
            int razor_val = -quiesce(st, -beta, -alpha, opp(cpu_player), cpu_player);
            if (razor_val >= beta) return razor_val;
        }
    }

    // ── Probcut: shallow search at elevated beta to skip cut nodes ───────
    if (pruning_safe && !pv_node && depth >= 5 && null_ok &&
        std::abs(beta) < 30000) {
        int probcut_beta = beta + 200;
        int probcut_depth = depth - 4;
        if (probcut_depth < 1) probcut_depth = 1;
        // Quick check: if static eval already beats probcut_beta, likely a cut
        if (node_is_max && static_eval >= probcut_beta) {
            int pc_val = alphabeta(st, probcut_depth, probcut_beta - 1, probcut_beta,
                                   node_is_max, cpu_player, ply, false, prev_move, td);
            if (pc_val >= probcut_beta) return pc_val;
        }
        if (!node_is_max && static_eval <= alpha - 200) {
            int probcut_alpha = alpha - 200;
            int pc_val = alphabeta(st, probcut_depth, probcut_alpha, probcut_alpha + 1,
                                   node_is_max, cpu_player, ply, false, prev_move, td);
            if (pc_val <= probcut_alpha) return pc_val;
        }
    }

    // ── Dynamic Null Move Pruning tuned for 11x12 volatility ──────────────
    if (null_ok && depth >= 3 && !pv_node) {
        int stm_pieces = 0;
        for (const auto& p : st.pieces) if (p.player == st.turn) stm_pieces++;

        // Disable NMP in likely zugzwang-ish reduced-material states.
        bool zugzwang_risk = side_has_only_pawn_militia_material(st.pieces, st.turn);
        if (stm_pieces > 2 && !zugzwang_risk) {
            int eval_margin = node_is_max ? (static_eval - beta) : (alpha - static_eval);

            // If static eval is far below bound, null move is unlikely to cut.
            if (eval_margin >= -64) {
                int cmd_tension = commander_attackers_cached(st, st.turn) +
                                  commander_attackers_cached(st, opp(st.turn));
                bool volatile_pos = (cmd_tension > 0);

                int R = 2;
                if (depth >= 10 && eval_margin >= 320) R = 4;
                else if (depth >= 7 && eval_margin >= 140) R = 3;

                // In complex/volatile or low-material positions use conservative reduction.
                if (volatile_pos || stm_pieces <= 7) R = 2;
                if (R > depth - 1) R = depth - 1;

                UndoMove nu;
                SearchState& ns = st;
                nu.turn_before = ns.turn;
                nu.hash_before = ns.hash;
                nu.quick_eval_before = ns.quick_eval;
                ns.hash ^= g_ZobristTurn[ns.turn=="red" ? 0 : 1];
                ns.turn = opp(ns.turn);
                ns.hash ^= g_ZobristTurn[ns.turn=="red" ? 0 : 1];
                ns.atk.valid = false;

                int null_val;
                if (node_is_max) {
                    null_val = -alphabeta(ns, depth-1-R, -beta, -beta+1,
                                          false, cpu_player, ply+1, false, prev_move, td);
                } else {
                    null_val = alphabeta(ns, depth-1-R, alpha, alpha+1,
                                         true, cpu_player, ply+1, false, prev_move, td);
                }
                ns.turn = nu.turn_before;
                ns.hash = nu.hash_before;
                ns.quick_eval = nu.quick_eval_before;
                ns.atk.valid = false;

                if (node_is_max) {
                    if (null_val >= beta) {
                        if (depth >= 8) {
                            int verify = alphabeta(st, depth-R-1, beta-1, beta,
                                                   node_is_max, cpu_player, ply+1, false, prev_move, td);
                            if (verify >= beta) return beta;
                        } else {
                            return beta;
                        }
                    }
                } else {
                    if (null_val <= alpha) {
                        if (depth >= 8) {
                            int verify = alphabeta(st, depth-R-1, alpha, alpha+1,
                                                   node_is_max, cpu_player, ply+1, false, prev_move, td);
                            if (verify <= alpha) return alpha;
                        } else {
                            return alpha;
                        }
                    }
                }
            }
        }
    }

    int pre_cpu_cmd_atk = commander_attackers_cached(st, cpu_player);
    int pre_opp_cmd_atk = commander_attackers_cached(st, opp(cpu_player));
    int pre_my_navy = st.navy_count[(cpu_player == "red") ? 0 : 1];
    AllMoves moves = all_moves_for(st.pieces, st.turn);
    if (moves.empty()) {
        build_attack_cache(st);
        return board_score(st.pieces, cpu_player, &st.atk, &st.turn);
    }

    const MoveTriple* pv_move_ptr = nullptr;
    MoveTriple pv_move_buf{};
    if (ply < MAX_PLY && (td ? td->pv_len[ply] : g_pv_len[ply]) > ply) {
        pv_move_buf = td ? td->pv[ply][ply] : g_pv[ply][ply];
        pv_move_ptr = &pv_move_buf;
    }
    moves = order_moves(moves, st.pieces, st.turn, ply, hash_move_ptr, pv_move_ptr, prev_move, td);

    int val = node_is_max ? -999999 : 999999;
    MoveTriple best_move = moves[0];
    int move_index = 0;
    int hist_pl = player_idx(st.turn);
    if (hist_pl < 0) hist_pl = 0;

    // Track quiet moves searched (for history malus on cutoff)
    struct QuietEntry { int ki; int dc; int dr; };
    std::vector<QuietEntry> searched_quiets;
    searched_quiets.reserve(16);

    for (auto& m : moves) {
        if (time_up()) break;
        int moved_idx0 = find_piece_idx_by_id(st.pieces, m.pid);
        int moved_ki = (moved_idx0 >= 0) ? kind_index(st.pieces[moved_idx0].kind) : -1;
        const Piece* target = piece_at_c(st.pieces, m.dc, m.dr);
        bool is_capture = (target && target->player != st.turn);
        const bool captures_navy = is_capture && target && target->kind=="N";
        bool is_critical_capture = is_capture &&
            (target->kind=="C" || target->kind=="N" || target->kind=="Af" ||
             target->kind=="A" || target->kind=="T" || target->kind=="In");
        int full_depth = depth - 1 + ((is_critical_capture && depth <= 4) ? 1 : 0);
        if (full_depth < 0) full_depth = 0;

        bool is_killer  = (ply<MAX_PLY &&
                           (((td ? td->killers_set[ply][0] : g_killers_set[ply][0]) &&
                             same_move(td ? td->killers[ply][0] : g_killers[ply][0], m)) ||
                            ((td ? td->killers_set[ply][1] : g_killers_set[ply][1]) &&
                             same_move(td ? td->killers[ply][1] : g_killers[ply][1], m))));

        bool is_hash_move = (hash_move_ptr && same_move(*hash_move_ptr, m));
        bool is_quiet = (!is_capture && !is_killer && !is_critical_capture && !is_hash_move);

        // ── Late Move Pruning (LMP) — improving-aware thresholds ─────────
        if (is_quiet && depth <= 4 && !pv_node) {
            int lmp_base = improving ? 5 : 3;
            int lmp_threshold = lmp_base + depth * depth;
            if (move_index >= lmp_threshold && pre_cpu_cmd_atk == 0 && pre_opp_cmd_atk == 0) {
                move_index++;
                continue;
            }
        }

        // ── History-Based Pruning ─────────────────────────────────────────
        // Skip quiet moves at shallow depth whose history score is strongly
        // negative — the engine has repeatedly found them to be bad moves.
        // Adapted from Stockfish's history pruning; gains ~6 Elo by cutting
        // ~8% of nodes at low depths with almost no accuracy loss.
        if (is_quiet && depth <= 6 && !pv_node && move_index > 1 && moved_ki >= 0 &&
            pre_cpu_cmd_atk == 0 && pre_opp_cmd_atk == 0) {
            int hval = td ? td_history_score(*td, hist_pl, moved_ki, m.dc, m.dr)
                          : history_score(hist_pl, moved_ki, m.dc, m.dr);
            if (hval < -55 * depth * depth) {
                move_index++;
                continue;
            }
        }

        // ── Futility Pruning (both sides, depth 1-3) ────────────────────
        if (is_quiet && !pv_node && depth <= 3 && pre_cpu_cmd_atk == 0 && pre_opp_cmd_atk == 0) {
            int fut_margin = (improving ? 130 : 170) * depth + 80;
            if (node_is_max && static_eval + fut_margin <= alpha) {
                move_index++;
                continue;
            }
            if (!node_is_max && static_eval - fut_margin >= beta) {
                move_index++;
                continue;
            }
        }

        // ── SEE Pruning: skip losing captures at shallow depth ──────────
        if (is_capture && !is_critical_capture && depth <= 4 && !pv_node && move_index > 0) {
            int see_val = see(st.pieces, m.dc, m.dr, st.turn);
            if (see_val < -80 * depth) {
                move_index++;
                continue;
            }
        }

        // ── Singular / Negative / Double Extension logic ──────────────────
        // Inspired by Stockfish's SE + negative extension combo (+8-12 Elo).
        //
        // For the TT-best (hash) move:
        //   • Singular (+1): no other move beats tte->val - 90 at depth-2.
        //   • Double-singular (+2): singular AND no move even came close
        //     (near-miss count == 0 after testing >= 4 alternatives).
        //
        // For non-hash moves when we have a reliable TT entry:
        //   • Negative (-1): TT says another move clearly fails high, so
        //     this move is unlikely to be the best — reduce its search.
        //   • Mild negative (-1): TT value is close to beta.
        //
        // se_extension is applied additively to rule_ext below.
        int se_extension = 0;
        {
            const int tt_val = tte ? tte->val : 0;
            if (is_hash_move && tte && tte->depth >= depth - 1 && depth >= 5 &&
                !time_up() && std::abs(tt_val) < 30000) {
                int sing_beta = tt_val - 90;
                bool is_singular = true;
                int tested = 0, near_miss = 0;
                for (auto& om : moves) {
                    if (same_move(om, m)) continue;
                    if (tested >= 16 || time_up()) break;
                    UndoMove su;
                    if (!make_move_inplace(st, om, cpu_player, su)) continue;
                    int sv = alphabeta(st, depth - 2, sing_beta - 1, sing_beta,
                                       false, cpu_player, ply + 1, false, &om, td);
                    unmake_move_inplace(st, su);
                    ++tested;
                    if (sv >= sing_beta) { is_singular = false; break; }
                    if (sv >= sing_beta - 30) ++near_miss;
                }
                if (is_singular) {
                    // Double extension: no alternative came even close
                    bool doubly_singular = (near_miss == 0 && tested >= 4 && !pv_node);
                    se_extension = doubly_singular ? 2 : 1;
                }
            } else if (!is_hash_move && tte && depth >= 5 &&
                       std::abs(tt_val) < 30000 && tte->flag == TT_LOWER) {
                // Negative extension: TT reports another move scores >= beta here,
                // so this non-best move is unlikely to matter — search it less.
                if (tt_val >= beta)       se_extension = -2;
                else if (tt_val >= beta - 60) se_extension = -1;
            }
        }

        UndoMove u;
        if (!make_move_inplace(st, m, cpu_player, u)) continue;
        tt_prefetch(st.hash);  // prefetch child's TT entry to hide latency

        // Rule-aware selective extensions.
        int post_cpu_cmd_atk = commander_attackers_cached(st, cpu_player);
        int post_opp_cmd_atk = commander_attackers_cached(st, opp(cpu_player));
        int post_my_navy = st.navy_count[(cpu_player == "red") ? 0 : 1];
        int rule_ext = 0;
        if (pre_cpu_cmd_atk > 0 && post_cpu_cmd_atk < pre_cpu_cmd_atk) rule_ext++;
        if (node_is_max && post_opp_cmd_atk > 0) rule_ext++;
        if (captures_navy) rule_ext++;
        if (pre_my_navy == 1 && post_my_navy == 1 && post_cpu_cmd_atk == 0) rule_ext++;
        // Singular / double-singular: add se_extension (1 or 2).
        // Negative extension handled separately below.
        if (se_extension > 0) rule_ext += se_extension;
        // Recapture extension: resolving captures on the same square as the
        // previous move prevents horizon-effect blunders in tactical sequences.
        if (prev_move && is_capture &&
            m.dc == prev_move->dc && m.dr == prev_move->dr) rule_ext++;
        if (rule_ext > 2) rule_ext = 2;

        int ext_depth = full_depth + rule_ext;
        // Apply negative extension after positive cap — keeps it meaningful.
        if (se_extension < 0) ext_depth = std::max(0, ext_depth + se_extension);
        if (ext_depth >= depth) ext_depth = depth - 1;
        if (ext_depth < 0) ext_depth = 0;
        int child;
        if (move_index == 0) {
            // PV move: full window
            child = alphabeta(st, ext_depth, alpha, beta, !node_is_max, cpu_player, ply+1, true, &m, td);
        } else {
            int new_depth = ext_depth;

            // ── LMR: table-driven reductions for late quiet moves ────────
            if (is_quiet && move_index >= 2 && depth >= 2) {
                int R = lmr_reduction(depth, move_index);
                if (pv_node) R -= 1;
                if (improving) R -= 1;
                if (!improving && depth >= 6) R += 1;
                if (R < 0) R = 0;
                new_depth = ext_depth - R;
                if (new_depth < 1) new_depth = 1;
            }

            // Zero-window (PVS) search
            if (node_is_max)
                child = alphabeta(st, new_depth, alpha, alpha+1, !node_is_max, cpu_player, ply+1, true, &m, td);
            else
                child = alphabeta(st, new_depth, beta-1,  beta,  !node_is_max, cpu_player, ply+1, true, &m, td);

            // Re-search at full depth if LMR-reduced search beats the bound
            bool lmr_fail = node_is_max ? (child > alpha) : (child < beta);
            if (new_depth < ext_depth && lmr_fail) {
                if (pv_node) {
                    // PV node: re-search directly with full window (skip extra ZW)
                    child = alphabeta(st, ext_depth, alpha, beta, !node_is_max, cpu_player, ply+1, true, &m, td);
                } else {
                    if (node_is_max)
                        child = alphabeta(st, ext_depth, alpha, alpha+1, !node_is_max, cpu_player, ply+1, true, &m, td);
                    else
                        child = alphabeta(st, ext_depth, beta-1, beta, !node_is_max, cpu_player, ply+1, true, &m, td);
                }
            }

            // Full-window re-search if PVS fails in PV node (only needed when LMR wasn't already re-searched full)
            if (!lmr_fail || new_depth >= ext_depth) {
                bool pvs_fail = node_is_max ? (child > alpha && child < beta)
                                            : (child < beta  && child > alpha);
                if (pvs_fail && pv_node) {
                    child = alphabeta(st, ext_depth, alpha, beta, !node_is_max, cpu_player, ply+1, true, &m, td);
                }
            }
        }

        unmake_move_inplace(st, u);

        // Track searched quiet moves for history malus
        if (is_quiet && moved_ki >= 0) {
            searched_quiets.push_back({moved_ki, m.dc, m.dr});
        }

        move_index++;

        if (node_is_max) {
            if (child > val) {
                val=child; best_move=m;
                if (ply < MAX_PLY) {
                    auto& pv_arr = td ? td->pv : g_pv;
                    auto& pv_l   = td ? td->pv_len : g_pv_len;
                    pv_arr[ply][ply] = m;
                    pv_l[ply] = ply + 1;
                    if (ply+1 < MAX_PLY && pv_l[ply+1] > ply+1) {
                        for (int i = ply+1; i < pv_l[ply+1] && i < MAX_PLY; i++)
                            pv_arr[ply][i] = pv_arr[ply+1][i];
                        pv_l[ply] = pv_l[ply+1];
                    }
                }
            }
            alpha = std::max(alpha, val);
            if (beta <= alpha) {
                if (!is_capture) {
                    if (td) { td_store_killer(*td, m, ply); } else { store_killer(m, ply); }
                    if (moved_ki >= 0) {
                        if (td) { td_update_history(*td, hist_pl, moved_ki, m.dc, m.dr, depth);
                                  td_update_cont_history(*td, prev_move, moved_ki, m.dc, m.dr, depth); }
                        else    { update_history(hist_pl, moved_ki, m.dc, m.dr, depth);
                                  update_cont_history(prev_move, moved_ki, m.dc, m.dr, depth); }
                    }
                    // History malus: penalise other quiet moves that didn't cause cutoff
                    for (auto& sq : searched_quiets) {
                        if (sq.ki == moved_ki && sq.dc == m.dc && sq.dr == m.dr) continue;
                        if (td) td_penalise_history(*td, hist_pl, sq.ki, sq.dc, sq.dr, depth);
                        else    penalise_history(hist_pl, sq.ki, sq.dc, sq.dr, depth);
                    }
                    if (prev_move && on_board(prev_move->dc, prev_move->dr)) {
                        if (td) { td->counter[prev_move->dc][prev_move->dr] = m;
                                  td->counter_set[prev_move->dc][prev_move->dr] = true; }
                        else    { g_counter[prev_move->dc][prev_move->dr] = m;
                                  g_counter_set[prev_move->dc][prev_move->dr] = true; }
                    }
                }
                break;
            }
        } else {
            if (child < val) {
                val=child; best_move=m;
                if (ply < MAX_PLY) {
                    auto& pv_arr = td ? td->pv : g_pv;
                    auto& pv_l   = td ? td->pv_len : g_pv_len;
                    pv_arr[ply][ply] = m;
                    pv_l[ply] = ply + 1;
                    if (ply+1 < MAX_PLY && pv_l[ply+1] > ply+1) {
                        for (int i = ply+1; i < pv_l[ply+1] && i < MAX_PLY; i++)
                            pv_arr[ply][i] = pv_arr[ply+1][i];
                        pv_l[ply] = pv_l[ply+1];
                    }
                }
            }
            beta = std::min(beta, val);
            if (beta <= alpha) {
                if (!is_capture) {
                    if (td) { td_store_killer(*td, m, ply); } else { store_killer(m, ply); }
                    if (moved_ki >= 0) {
                        if (td) { td_update_history(*td, hist_pl, moved_ki, m.dc, m.dr, depth);
                                  td_update_cont_history(*td, prev_move, moved_ki, m.dc, m.dr, depth); }
                        else    { update_history(hist_pl, moved_ki, m.dc, m.dr, depth);
                                  update_cont_history(prev_move, moved_ki, m.dc, m.dr, depth); }
                    }
                    for (auto& sq : searched_quiets) {
                        if (sq.ki == moved_ki && sq.dc == m.dc && sq.dr == m.dr) continue;
                        if (td) td_penalise_history(*td, hist_pl, sq.ki, sq.dc, sq.dr, depth);
                        else    penalise_history(hist_pl, sq.ki, sq.dc, sq.dr, depth);
                    }
                    if (prev_move && on_board(prev_move->dc, prev_move->dr)) {
                        if (td) { td->counter[prev_move->dc][prev_move->dr] = m;
                                  td->counter_set[prev_move->dc][prev_move->dr] = true; }
                        else    { g_counter[prev_move->dc][prev_move->dr] = m;
                                  g_counter_set[prev_move->dc][prev_move->dr] = true; }
                    }
                }
                break;
            }
        }
    }

    if (move_index == 0) {
        build_attack_cache(st);
        return board_score(st.pieces, cpu_player, &st.atk, &st.turn);
    }

    int flag = (val<=orig_alpha) ? TT_UPPER : (val>=orig_beta ? TT_LOWER : TT_EXACT);
    tt_store(h, depth, flag, val, best_move);
    return val;
}

struct AIResult { bool found; MoveTriple move; };

// ═══════════════════════════════════════════════════════════════════════════
// HYBRID MCTS + ALPHA-BETA ROOT SEARCH  (AlphaZero-style)
// ═══════════════════════════════════════════════════════════════════════════
//
// Architecture:
//   • Root:  Monte-Carlo Tree Search with PUCT selection guided by a
//            heuristic "policy head" (captures/mobility/positional priors).
//   • Leaf:  Alpha-beta at MCTS_AB_DEPTH plies — acts as the "value head".
//   • This eliminates the horizon effect that pure AB suffers at the root:
//     promising moves receive exponentially more AB evaluations, naturally
//     extending the effective search horizon.
// ────────────────────────────────────────────────────────────────────────────

static constexpr float MCTS_CPUCT    = 1.8f;  // exploration constant
static constexpr int   MCTS_AB_DEPTH = 3;     // AB plies per leaf evaluation (was 5; 3 gives ~2× more sims)
static constexpr float MCTS_VIRTUAL_LOSS = 0.35f;
static constexpr int   MCTS_MAX_THREADS = 8;
static constexpr int   MCTS_EVAL_BATCH_CPU = 16;
static constexpr int   MCTS_EVAL_BATCH_WEBGPU = 128;

// ── Heuristic policy prior (simulates NNUE policy head) ──────────────────
// Returns a softmax probability vector over the given moves.
static std::vector<float> mcts_policy_priors(const AllMoves& moves,
                                              const PieceList& pieces,
                                              const std::string& player) {
    if (moves.empty()) return {};
    std::vector<float> raw(moves.size(), 0.0f);
    int hist_pl = player_idx(player);
    if (hist_pl < 0) hist_pl = 0;

    // Pre-locate commanders for threat/escape bonuses (O(n) once).
    const Piece* my_cmd  = nullptr;
    const Piece* opp_cmd = nullptr;
    for (const auto& p : pieces) {
        if (p.kind != "C") continue;
        if (p.player == player) my_cmd = &p;
        else                    opp_cmd = &p;
    }

    for (size_t i = 0; i < moves.size(); i++) {
        const auto& m = moves[i];
        float s = 0.0f;

        // Capture bonus: MVV/LVA-weighted with SEE refinement
        const Piece* tgt = piece_at_c(pieces, m.dc, m.dr);
        if (tgt && tgt->player != player) {
            int victim = piece_value_fast(tgt->kind);
            int atk_idx = find_piece_idx_by_id(pieces, m.pid);
            int attacker = (atk_idx >= 0) ? std::max(1, piece_value_fast(pieces[atk_idx].kind)) : 1;
            s += 300.0f + victim * 2.0f - attacker * 0.25f;
            int sv = see(pieces, m.dc, m.dr, player);
            s += sv >= 0 ? (50.0f + sv * 0.05f) : (sv * 0.02f);
        }

        // Central-control bonus
        float cdist = std::abs(m.dc - 5) + std::abs(m.dr - 6);
        s += std::max(0.0f, 18.0f - cdist * 2.5f);

        // Forward-advance bonus (push pieces toward opponent)
        int atk_idx = find_piece_idx_by_id(pieces, m.pid);
        if (atk_idx >= 0) {
            const Piece& ap = pieces[atk_idx];
            float adv = (player == "blue") ? (float)(ap.row - m.dr)
                                           : (float)(m.dr - ap.row);
            s += adv * 3.5f;
        }

        // History heuristic bonus from global table
        if (atk_idx >= 0 && atk_idx < (int)pieces.size()) {
                int ki = kind_index(pieces[atk_idx].kind);
                if (ki >= 0 && ki < H_KINDS) {
                    int hist = g_history[hist_pl][ki][m.dc][m.dr];
                    s += (float)hist * 0.008f;
                }
            }

        // ── Commander threat bonus ────────────────────────────────────────
        // Moves that bring a piece close to the enemy commander or that
        // directly threaten to capture it are far more important in
        // Commander Chess than central control alone.
        if (opp_cmd) {
            int dist = std::abs(m.dc - opp_cmd->col) + std::abs(m.dr - opp_cmd->row);
            if (dist == 0)      s += 800.0f;  // captures commander (shouldn't happen normally)
            else if (dist <= 1) s += 350.0f;  // adjacent — direct threat
            else if (dist <= 2) s += 180.0f;  // very close
            else if (dist <= 4) s +=  60.0f;  // approaching
        }

        // ── Own-commander shelter / escape bonus ──────────────────────────
        // When our commander is threatened, prioritise moves that place a
        // piece as a bodyguard (nearby defender) or move commander to safety.
        if (my_cmd) {
            bool my_cmd_piece = (atk_idx >= 0 && pieces[atk_idx].kind == "C");
            int dist_to = std::abs(m.dc - my_cmd->col) + std::abs(m.dr - my_cmd->row);
            if (my_cmd_piece) {
                // Commander moving: bonus for moving away from danger
                s += 30.0f;
            } else if (dist_to <= 2 && atk_idx >= 0) {
                int dist_from = std::abs(pieces[atk_idx].col - my_cmd->col)
                              + std::abs(pieces[atk_idx].row - my_cmd->row);
                if (dist_to < dist_from) s += 40.0f; // moving toward own commander = shelter
            }
        }

        raw[i] = s;
    }

    // Softmax with temperature τ = 25 (was 80).
    // Sharper temperature focuses simulations on stronger moves while
    // still preserving enough exploration for PUCT to function correctly.
    float max_s = *std::max_element(raw.begin(), raw.end());
    float sum = 0.0f;
    for (auto& v : raw) { v = std::exp((v - max_s) / 25.0f); sum += v; }
    if (sum > 1e-9f) for (auto& v : raw) v /= sum;
    return raw;
}

// ── Per-child node at the MCTS root ──────────────────────────────────────
struct MCTSRootChild {
    MoveTriple  move{};
    float       prior       = 0.0f;
    int         visits      = 0;
    int         virtual_loss = 0;
    float       total_value = 0.0f;
    SearchState state;                  // board snapshot after this move

    float q() const {
        return visits > 0 ? total_value / (float)visits : 0.0f;
    }
};

// ── Two-level MCTS: root children + their children ───────────────────────
// Each root child can be further expanded one more ply.
struct MCTSLevel2Child {
    MoveTriple  move{};
    float       prior       = 0.0f;
    int         visits      = 0;
    int         virtual_loss = 0;
    float       total_value = 0.0f;
    SearchState state;
    float q() const { return visits > 0 ? total_value / (float)visits : 0.0f; }
    float q_with_virtual_loss() const {
        int v = visits + virtual_loss;
        if (v <= 0) return 0.0f;
        float tv = total_value - MCTS_VIRTUAL_LOSS * (float)virtual_loss;
        return tv / (float)v;
    }
    int visits_with_virtual_loss() const { return visits + virtual_loss; }
};

struct MCTSLevel1Child {
    MoveTriple  move{};
    float       prior       = 0.0f;
    int         visits      = 0;
    int         virtual_loss = 0;
    float       total_value = 0.0f;
    SearchState state;
    bool        expanded    = false;
    std::vector<MCTSLevel2Child> children;

    float q() const { return visits > 0 ? total_value / (float)visits : 0.0f; }
    float q_with_virtual_loss() const {
        int v = visits + virtual_loss;
        if (v <= 0) return 0.0f;
        float tv = total_value - MCTS_VIRTUAL_LOSS * (float)virtual_loss;
        return tv / (float)v;
    }
    int visits_with_virtual_loss() const { return visits + virtual_loss; }
};

// ── Main MCTS+AB root search ──────────────────────────────────────────────
static AIResult mcts_ab_root_search(const PieceList& pieces,
                                     const std::string& cpu_player,
                                     int ab_depth,
                                     double time_limit_secs,
                                     const std::atomic<bool>* stop_flag = nullptr) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds((long long)(time_limit_secs * 1000));
    g_deadline = deadline;
    g_stop_flag = stop_flag;
    reset_time_state();
    g_nodes.store(0, std::memory_order_relaxed);

    SearchState root_st = make_search_state(pieces, cpu_player, cpu_player);
    AllMoves all_moves  = all_moves_for(root_st.pieces, cpu_player);
    if (all_moves.empty()) return {false, {}};
    if (all_moves.size() == 1) return {true, all_moves[0]};

    std::vector<float> root_priors =
        mcts_policy_priors(all_moves, root_st.pieces, cpu_player);

    std::vector<MCTSLevel1Child> children;
    children.reserve(all_moves.size());
    for (size_t i = 0; i < all_moves.size(); i++) {
        MCTSLevel1Child c;
        c.move  = all_moves[i];
        c.prior = root_priors[i];
        c.state = root_st;
        UndoMove u;
        if (!make_move_inplace(c.state, all_moves[i], cpu_player, u)) continue;
        children.push_back(std::move(c));
    }
    if (children.empty()) return {false, {}};

    struct SelectionPath {
        int l1_idx = -1;
        int l2_idx = -1;
        SearchState eval_st;
        MoveTriple prev_move{};
    };

    int root_visits = 1;
    std::mutex tree_mutex;
    const std::string opp_player = opp(cpu_player);

    auto evaluate_leaf_value = [&](SelectionPath& sel, int* out_val) -> bool {
        if (!out_val) return false;
        int val = alphabeta(sel.eval_st, ab_depth, -999999, 999999,
                            (sel.eval_st.turn == cpu_player),
                            cpu_player, (sel.l2_idx >= 0 ? 2 : 1), true, &sel.prev_move, nullptr);
        if (time_up()) return false;
        *out_val = val;
        return true;
    };

    auto apply_leaf_result = [&](const SelectionPath& sel, float leaf_val) -> bool {
        std::lock_guard<std::mutex> lk(tree_mutex);
        if (sel.l1_idx < 0 || sel.l1_idx >= (int)children.size()) return false;
        MCTSLevel1Child& l1 = children[sel.l1_idx];
        if (l1.virtual_loss > 0) l1.virtual_loss--;

        if (sel.l2_idx >= 0 && sel.l2_idx < (int)l1.children.size()) {
            MCTSLevel2Child& l2 = l1.children[sel.l2_idx];
            if (l2.virtual_loss > 0) l2.virtual_loss--;
            l2.visits++;
            // Level-2 node is opponent-to-move; store from opponent perspective.
            l2.total_value -= leaf_val;
        }

        l1.visits++;
        l1.total_value += leaf_val;
        root_visits++;
        return true;
    };

    auto rollback_virtual_loss = [&](const SelectionPath& sel) {
        std::lock_guard<std::mutex> lk(tree_mutex);
        if (sel.l1_idx < 0 || sel.l1_idx >= (int)children.size()) return;
        MCTSLevel1Child& l1 = children[sel.l1_idx];
        if (l1.virtual_loss > 0) l1.virtual_loss--;
        if (sel.l2_idx >= 0 && sel.l2_idx < (int)l1.children.size()) {
            MCTSLevel2Child& l2 = l1.children[sel.l2_idx];
            if (l2.virtual_loss > 0) l2.virtual_loss--;
        }
    };

    auto select_path = [&](SelectionPath& sel) -> bool {
        std::lock_guard<std::mutex> lk(tree_mutex);
        if (children.empty()) return false;

        float sqrt_root = std::sqrt((float)std::max(1, root_visits));
        int l1_idx = 0;
        float best_puct = -1e18f;
        for (int i = 0; i < (int)children.size(); i++) {
            auto& c = children[i];
            float q = c.q_with_virtual_loss();
            float u = MCTS_CPUCT * c.prior * sqrt_root /
                      (1.0f + (float)c.visits_with_virtual_loss());
            float puct = q + u;
            if (puct > best_puct) { best_puct = puct; l1_idx = i; }
        }

        MCTSLevel1Child& l1 = children[l1_idx];
        l1.virtual_loss++;
        sel.l1_idx = l1_idx;
        sel.eval_st = l1.state;
        sel.prev_move = l1.move;

        if (l1.visits >= 2) {
            if (!l1.expanded) {
                l1.expanded = true;
                AllMoves l2_moves = all_moves_for(l1.state.pieces, opp_player);
                std::vector<float> l2_priors =
                    mcts_policy_priors(l2_moves, l1.state.pieces, opp_player);
                l1.children.reserve(l2_moves.size());
                for (size_t j = 0; j < l2_moves.size(); j++) {
                    MCTSLevel2Child c2;
                    c2.move  = l2_moves[j];
                    c2.prior = l2_priors[j];
                    c2.state = l1.state;
                    UndoMove u2;
                    if (!make_move_inplace(c2.state, l2_moves[j], opp_player, u2)) continue;
                    l1.children.push_back(std::move(c2));
                }
            }

            if (!l1.children.empty()) {
                float sqrt_l1 = std::sqrt((float)std::max(1, l1.visits_with_virtual_loss()));
                int l2_idx = 0;
                float best_puct2 = -1e18f;
                for (int j = 0; j < (int)l1.children.size(); j++) {
                    auto& c2 = l1.children[j];
                    float q2 = c2.q_with_virtual_loss();
                    float u2 = MCTS_CPUCT * c2.prior * sqrt_l1 /
                               (1.0f + (float)c2.visits_with_virtual_loss());
                    // q2 is stored from opponent perspective at level-2.
                    float puct2 = q2 + u2;
                    if (puct2 > best_puct2) { best_puct2 = puct2; l2_idx = j; }
                }
                MCTSLevel2Child& l2 = l1.children[l2_idx];
                l2.virtual_loss++;
                sel.l2_idx = l2_idx;
                sel.eval_st = l2.state;
                sel.prev_move = l2.move;
            }
        }
        return true;
    };

    auto worker = [&](int tid) {
        g_deadline = deadline;
        g_stop_flag = stop_flag;
        reset_time_state();
        (void)tid;

        const bool use_webgpu = (active_eval_backend() == EvalBackendKind::WEBGPU);
        int eval_batch_size = use_webgpu ? MCTS_EVAL_BATCH_WEBGPU : MCTS_EVAL_BATCH_CPU;
        eval_batch_size = std::max(1, std::min(eval_batch_size, (int)children.size()));

        while (!time_up()) {
            if (stop_flag && stop_flag->load(std::memory_order_relaxed)) break;

            std::vector<SelectionPath> selected;
            selected.reserve((size_t)eval_batch_size);
            for (int i = 0; i < eval_batch_size && !time_up(); i++) {
                SelectionPath sel;
                if (!select_path(sel)) break;
                selected.push_back(std::move(sel));
            }
            if (selected.empty()) break;

            std::vector<int> values(selected.size(), 0);
            std::vector<uint8_t> ok(selected.size(), 0);
            for (size_t i = 0; i < selected.size(); i++) {
                int val = 0;
                if (evaluate_leaf_value(selected[i], &val)) {
                    values[i] = val;
                    ok[i] = 1;
                }
            }

            // Node queue batching path: blend AB leaf values with batched evaluator scores.
            std::vector<EvalBatchRequest> reqs;
            std::vector<size_t> req_idx;
            reqs.reserve(selected.size());
            req_idx.reserve(selected.size());
            for (size_t i = 0; i < selected.size(); i++) {
                if (!ok[i]) continue;
                build_attack_cache(selected[i].eval_st);
                reqs.push_back(EvalBatchRequest{
                    &selected[i].eval_st.pieces,
                    &cpu_player,
                    &selected[i].eval_st.atk,
                    &selected[i].eval_st.turn
                });
                req_idx.push_back(i);
            }
            if (!reqs.empty()) {
                std::vector<int> batch_scores = board_score_batch(reqs);
                size_t n = std::min(batch_scores.size(), req_idx.size());
                for (size_t j = 0; j < n; j++) {
                    size_t idx = req_idx[j];
                    if (use_webgpu) values[idx] = (values[idx] * 3 + batch_scores[j]) / 4;
                    else            values[idx] = (values[idx] * 7 + batch_scores[j]) / 8;
                }
            }

            bool any_applied = false;
            for (size_t i = 0; i < selected.size(); i++) {
                if (!ok[i]) {
                    rollback_virtual_loss(selected[i]);
                    continue;
                }
                float leaf_val = std::max(-1.0f, std::min(1.0f, (float)values[i] / 6000.0f));
                if (apply_leaf_result(selected[i], leaf_val)) {
                    any_applied = true;
                } else {
                    rollback_virtual_loss(selected[i]);
                }
            }
            if (!any_applied && time_up()) {
                break;
            }
        }
    };

    int hw_threads = (int)std::thread::hardware_concurrency();
    if (hw_threads <= 0) hw_threads = 1;
    int num_workers = std::max(1, std::min(hw_threads, MCTS_MAX_THREADS));
    if (time_limit_secs <= 0.10 || (int)children.size() <= 2) num_workers = 1;

    if (num_workers == 1) {
        worker(0);
    } else {
        std::vector<std::thread> threads;
        threads.reserve(num_workers);
        for (int i = 0; i < num_workers; i++) threads.emplace_back(worker, i);
        for (auto& t : threads) if (t.joinable()) t.join();
    }

    int best_idx = 0;
    int best_visits = -1;
    float best_q = -1e18f;
    for (int i = 0; i < (int)children.size(); i++) {
        const auto& c = children[i];
        if (c.visits > best_visits ||
            (c.visits == best_visits && c.q() > best_q)) {
            best_visits = c.visits;
            best_q = c.q();
            best_idx = i;
        }
    }

    if (best_visits <= 0) return {false, {}};
    return {true, children[best_idx].move};
}

static bool g_use_mcts = false;   // enabled at Hard difficulty

static bool is_legal_book_move(const SearchState& st, const std::string& cpu_player, const MoveTriple& cand) {
    int idx = find_piece_idx_by_id(st.pieces, cand.pid);
    if (idx < 0) return false;
    const Piece& p = st.pieces[idx];
    if (p.player != cpu_player) return false;
    auto mvs = get_moves(p, st.pieces);
    for (auto& mv : mvs) if (mv.first==cand.dc && mv.second==cand.dr) return true;
    return false;
}

static void append_book_move_from_square(std::vector<MoveTriple>& book,
                                         const SearchState& st,
                                         const std::string& cpu_player,
                                         int from_c, int from_r,
                                         int to_c, int to_r) {
    int idx = find_piece_idx_at(st.pieces, from_c, from_r);
    if (idx < 0) return;
    const Piece& p = st.pieces[idx];
    if (p.player != cpu_player) return;
    book.push_back({p.id, to_c, to_r});
}

static int opening_immediate_risk(const PieceList& pieces, const std::string& cpu_player) {
    AllMoves om = all_moves_for(pieces, opp(cpu_player));
    bool commander_hanging = false;
    std::set<int> af_hanging;
    std::set<int> navy_hanging;
    std::set<int> land_hanging;

    for (auto& m : om) {
        const Piece* t = piece_at_c(pieces, m.dc, m.dr);
        if (!t || t->player != cpu_player) continue;
        if (t->kind == "C") commander_hanging = true;
        else if (t->kind == "Af") af_hanging.insert(t->id);
        else if (t->kind == "N") navy_hanging.insert(t->id);
        else if (t->kind == "A" || t->kind == "T" || t->kind == "In")
            land_hanging.insert(t->id);
    }

    int risk = 0;
    if (commander_hanging) risk += 1000000;
    risk += (int)af_hanging.size() * 6000;
    risk += (int)navy_hanging.size() * 1400;
    risk += (int)land_hanging.size() * 250;
    return risk;
}

static bool opening_book_pick(const SearchState& st, const std::string& cpu_player, MoveTriple& out) {
    if (cpu_player != "blue") return false;
    if (st.pieces.size() < 34) return false; // opening only
    const bool very_early_opening = (st.pieces.size() >= 36);

    std::vector<MoveTriple> book;

    // Prefer early navy stabilization + air control in opening.
    const Piece* navy_back = piece_at_c(st.pieces, 0, 10);
    const Piece* navy_front = piece_at_c(st.pieces, 0, 8);
    bool have_back_navy = navy_back && navy_back->player == cpu_player && navy_back->kind == "N";
    bool have_front_navy = navy_front && navy_front->player == cpu_player && navy_front->kind == "N";
    if (have_back_navy && have_front_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, 8, 1, 8);
        append_book_move_from_square(book, st, cpu_player, 0, 10, 1, 10);
    } else if (have_back_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, 10, 1, 10);
    } else if (have_front_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, 8, 1, 8);
    }
    // Delay non-capture AF flights in the very first turns.
    if (!very_early_opening) {
        append_book_move_from_square(book, st, cpu_player, 3, 7, 2, 7);
        append_book_move_from_square(book, st, cpu_player, 7, 7, 8, 7);
        append_book_move_from_square(book, st, cpu_player, 3, 7, 3, 8);
        append_book_move_from_square(book, st, cpu_player, 7, 7, 7, 8);
    }
    append_book_move_from_square(book, st, cpu_player, 5, 7, 5, 6);
    append_book_move_from_square(book, st, cpu_player, 4, 8, 4, 7);
    append_book_move_from_square(book, st, cpu_player, 6, 8, 6, 7);

    bool found = false;
    int best_score = -99999999;
    MoveTriple best{};
    const std::string stm_after = opp(cpu_player);
    for (auto& m : book) {
        if (!is_legal_book_move(st, cpu_player, m)) continue;
        PieceList np = apply_move(st.pieces, m.pid, m.dc, m.dr, cpu_player);
        if (has_immediate_winning_move(np, opp(cpu_player))) continue;
        int risk = opening_immediate_risk(np, cpu_player);
        if (risk >= 1000000) continue; // never allow immediate commander hangs
        int score = board_score(np, cpu_player, nullptr, &stm_after) - risk;
        if (!found || score > best_score) {
            best_score = score;
            best = m;
            found = true;
        }
    }
    if (found) { out = best; return true; }
    return false;
}

static bool g_use_opening_book = true;

static AIResult cpu_pick_move(const PieceList& pieces, const std::string& cpu_player,
                               int max_depth, double time_limit_secs,
                               const std::atomic<bool>* stop_flag = nullptr,
                               ThreadData* td = nullptr) {
    struct StopFlagScope {
        const std::atomic<bool>* prev = nullptr;
        explicit StopFlagScope(const std::atomic<bool>* flag) : prev(g_stop_flag) { g_stop_flag = flag; }
        ~StopFlagScope() { g_stop_flag = prev; }
    } stop_scope(stop_flag);

    g_deadline = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds((int)(time_limit_secs * 1000));
    auto soft_deadline = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds((int)(time_limit_secs * 550));
    reset_time_state();
    // Seed search path with game-level repetition history so the engine
    // avoids moves that create threefold repetition with prior positions.
    g_search_hash_path = g_game_rep_history;  // may be empty if not in sim
    g_nodes.store(0, std::memory_order_relaxed);

    SearchState root = make_search_state(pieces, cpu_player, cpu_player);
    AllMoves all_moves = all_moves_for(root.pieces, cpu_player);
    if (all_moves.empty()) return {false, {}};
    if (all_moves.size() == 1) return {true, all_moves[0]};  // easy move

    MoveTriple book_move{};
    if (g_use_opening_book && opening_book_pick(root, cpu_player, book_move)) {
        return {true, book_move};
    }

    // Keep deterministic root order for stronger, reproducible play.

    MoveTriple best = all_moves[0];
    int prev_score  = 0;
    int move_stability = 0;
    const bool opening_phase = (root.pieces.size() >= 34);
    const bool very_early_opening = (root.pieces.size() >= 36);
    const int base_opening_risk = opening_phase ? opening_immediate_risk(root.pieces, cpu_player) : 0;

    for (int cur_depth = 1; cur_depth <= max_depth; cur_depth++) {
        if (time_up()) break;

        // ── Aspiration Windows ────────────────────────────────────────────
        // Start with a tight window (δ=12 at depth≥5, δ=40 earlier).
        // Widen ASYMMETRICALLY: only expand in the direction of the failure,
        // which halves wasted re-searches vs the old symmetric ×2 approach.
        int delta = (cur_depth >= 5) ? 12 : 40;
        int alpha = (cur_depth > 1) ? prev_score - delta : -999999;
        int beta  = (cur_depth > 1) ? prev_score + delta :  999999;

        MoveTriple cur_best = best;
        int cur_best_val    = -999999;  // raw search value (for aspiration logic)
        int cur_best_rank   = -999999;  // style-adjusted root ranking
        bool completed      = false;

        while (!time_up()) {
            cur_best_val = -999999;
            cur_best_rank = -999999;
            cur_best     = best;

            MoveTriple root_hash_buf{};
            const MoveTriple* root_hash_move = nullptr;
            const TTEntry* rt = tt_probe(root.hash);
            if (rt) { root_hash_buf = tt_unpack_move(*rt); root_hash_move = &root_hash_buf; }

            MoveTriple root_pv_buf{};
            const MoveTriple* root_pv_move = nullptr;
            int rpv_len = td ? td->pv_len[0] : g_pv_len[0];
            if (rpv_len > 0) {
                root_pv_buf = td ? td->pv[0][0] : g_pv[0][0];
                root_pv_move = &root_pv_buf;
            }

            AllMoves root_moves = order_moves(all_moves, root.pieces, cpu_player, 0,
                                              root_hash_move ? root_hash_move : &best,
                                              root_pv_move, nullptr, td);

            int window_alpha = alpha;
            int window_beta  = beta;
            int root_move_idx = 0;
            for (auto& m : root_moves) {
                if (time_up()) break;
                int moved_idx = find_piece_idx_by_id(root.pieces, m.pid);
                std::string moved_kind = (moved_idx >= 0) ? root.pieces[moved_idx].kind : "";
                const Piece* root_target = piece_at_c(root.pieces, m.dc, m.dr);
                bool root_is_capture = (root_target && root_target->player != cpu_player);

                UndoMove u;
                if (!make_move_inplace(root, m, cpu_player, u)) continue;

                int root_risk = 0;
                bool opp_immediate_win = false;
                if (opening_phase) {
                    root_risk = opening_immediate_risk(root.pieces, cpu_player);
                    opp_immediate_win = has_immediate_winning_move(root.pieces, opp(cpu_player));
                    if (root_risk >= 1000000) { // never allow immediate commander hangs
                        unmake_move_inplace(root, u);
                        continue;
                    }
                }

                int val;
                if (root_move_idx == 0) {
                    // First move: full window search
                    val = alphabeta(root, cur_depth-1, window_alpha, window_beta, false,
                                    cpu_player, 1, true, &m, td);
                } else {
                    // PVS: zero-window search first
                    val = alphabeta(root, cur_depth-1, window_alpha, window_alpha+1, false,
                                    cpu_player, 1, true, &m, td);
                    // Re-search with full window if it beats alpha but doesn't reach beta
                    if (val > window_alpha && val < window_beta) {
                        val = alphabeta(root, cur_depth-1, window_alpha, window_beta, false,
                                        cpu_player, 1, true, &m, td);
                    }
                }

                int style_penalty = 0;
                if (opening_phase) {
                    style_penalty += root_risk / (very_early_opening ? 3 : 6);
                    if (opp_immediate_win) style_penalty += 250000;
                    if (very_early_opening && root_risk > base_opening_risk + 4500)
                        style_penalty += 900;  // avoid lines that newly hang key units
                    if (moved_kind == "Af" && !root_is_capture)
                        style_penalty += very_early_opening ? 280 : 120;
                    if (moved_kind == "Af" && root_risk > base_opening_risk)
                        style_penalty += 180;
                }
                int ranked = val - style_penalty;

                unmake_move_inplace(root, u);
                if (val > cur_best_val) cur_best_val = val;
                if (ranked > cur_best_rank) { cur_best_rank = ranked; cur_best = m; }
                window_alpha = std::max(window_alpha, val);
                root_move_idx++;
                // Beta cutoff: no need to search remaining root moves
                if (val >= window_beta) break;
            }

            if (time_up()) break;

            if      (cur_best_val <= alpha) { /* fail-low */ }
            else if (cur_best_val >= beta ) { /* fail-high */ }
            else { completed = true; break; }  // inside window — done

            // Widen window and retry — ASYMMETRIC: only expand in the fail direction.
            // Growth factor 1.44 + 5 gives geometric widening that converges faster
            // than the old ×2 approach while avoiding thrashing near the true score.
            delta = (int)(delta * 1.44f) + 5;
            if (cur_best_val <= alpha) alpha = std::max(-999999, cur_best_val - delta);
            else                       beta  = std::min( 999999, cur_best_val + delta);
            if (delta > 800) break; // give up on aspiration, accept result
        }

        if (completed) {
            MoveTriple old_best = best;
            best = cur_best;
            // Track best-move stability
            if (same_move(best, old_best)) move_stability++;
            else move_stability = 0;
            prev_score = cur_best_val;
            // Soft-stop: stable best move past soft deadline
            if (move_stability >= 3 && cur_depth >= 4 &&
                std::chrono::steady_clock::now() > soft_deadline) {
                break;
            }
        }
    }
    return {true, best};
}

// ═══════════════════════════════════════════════════════════════════════════
// LAZY SMP — Multi-threaded search
// ═══════════════════════════════════════════════════════════════════════════

static int g_smp_thread_count = 0; // 0 = auto-detect

static int smp_thread_count() {
    if (g_smp_thread_count > 0) return g_smp_thread_count;
    int hw = (int)std::thread::hardware_concurrency();
    if (hw <= 0) hw = 1;
    // Use all available hardware threads by default.
    return std::max(hw, 1);
}

struct SMPShared {
    std::atomic<bool>   stop{false};
    std::atomic<int>    best_score{-999999};
    std::mutex          best_mutex;
    MoveTriple          best_move{};
    bool                best_found{false};
    std::chrono::steady_clock::time_point deadline;       // hard limit
    std::chrono::steady_clock::time_point soft_deadline;  // soft limit (stop if stable)
    std::atomic<int>    best_move_stability{0}; // consecutive iterations with same best move
    std::atomic<int>    last_best_pid{-1};
    std::atomic<int>    last_best_dc{-1};
    std::atomic<int>    last_best_dr{-1};
};

static void smp_worker(int thread_id, const PieceList& pieces,
                        const std::string& cpu_player,
                        int max_depth, SMPShared& shared) {
    init_lmr_table();
    reset_time_state();
    g_search_hash_path = g_game_rep_history;  // seed with game history
    // Each thread gets its own ThreadData
    ThreadData td;
    td.thread_id = thread_id;
    td.reset();

    // Set up stop flag and deadline for this thread
    g_deadline = shared.deadline;

    SearchState root = make_search_state(pieces, cpu_player, cpu_player);
    AllMoves all_moves = all_moves_for(root.pieces, cpu_player);
    if (all_moves.empty()) return;

    // Diversify move ordering: thread 0 uses normal order, others shuffle early moves
    if (thread_id > 0 && all_moves.size() > 2) {
        std::mt19937 rng(thread_id * 7919 + 42);
        // Shuffle only the first few moves to diversify while keeping structure
        int shuffle_count = std::min((int)all_moves.size(), 4 + thread_id);
        for (int i = 0; i < shuffle_count - 1; i++) {
            std::uniform_int_distribution<int> dist(i, shuffle_count - 1);
            std::swap(all_moves[i], all_moves[dist(rng)]);
        }
    }

    MoveTriple best = all_moves[0];
    int prev_score = 0;
    const bool opening_phase = (root.pieces.size() >= 34);
    const bool very_early_opening = (root.pieces.size() >= 36);
    const int base_opening_risk = opening_phase ? opening_immediate_risk(root.pieces, cpu_player) : 0;

    // Threads at higher IDs can start from a deeper depth to diversify
    int start_depth = 1 + (thread_id % 2); // odd threads skip depth 1

    for (int cur_depth = start_depth; cur_depth <= max_depth; cur_depth++) {
        if (shared.stop.load(std::memory_order_relaxed)) break;
        if (std::chrono::steady_clock::now() > shared.deadline) break;

        int delta = (cur_depth >= 5) ? 40 : 60;
        int alpha = (cur_depth > start_depth) ? prev_score - delta : -999999;
        int beta  = (cur_depth > start_depth) ? prev_score + delta :  999999;

        MoveTriple cur_best = best;
        int cur_best_val    = -999999;
        int cur_best_rank   = -999999;
        bool completed      = false;

        while (!shared.stop.load(std::memory_order_relaxed) &&
               std::chrono::steady_clock::now() <= shared.deadline) {
            cur_best_val = -999999;
            cur_best_rank = -999999;
            cur_best = best;

            MoveTriple root_hash_buf{};
            const MoveTriple* root_hash_move = nullptr;
            const TTEntry* rt = tt_probe(root.hash);
            if (rt) { root_hash_buf = tt_unpack_move(*rt); root_hash_move = &root_hash_buf; }

            MoveTriple root_pv_buf{};
            const MoveTriple* root_pv_move = nullptr;
            if (td.pv_len[0] > 0) { root_pv_buf = td.pv[0][0]; root_pv_move = &root_pv_buf; }

            AllMoves root_moves = order_moves(all_moves, root.pieces, cpu_player, 0,
                                              root_hash_move ? root_hash_move : &best,
                                              root_pv_move, nullptr, &td);

            int window_alpha = alpha;
            int window_beta  = beta;
            int root_move_idx = 0;

            for (auto& m : root_moves) {
                if (shared.stop.load(std::memory_order_relaxed)) break;
                if (std::chrono::steady_clock::now() > shared.deadline) break;

                int moved_idx = find_piece_idx_by_id(root.pieces, m.pid);
                std::string moved_kind = (moved_idx >= 0) ? root.pieces[moved_idx].kind : "";
                const Piece* root_target = piece_at_c(root.pieces, m.dc, m.dr);
                bool root_is_capture = (root_target && root_target->player != cpu_player);

                UndoMove u;
                if (!make_move_inplace(root, m, cpu_player, u)) continue;

                int root_risk = 0;
                bool opp_immediate_win = false;
                if (opening_phase) {
                    root_risk = opening_immediate_risk(root.pieces, cpu_player);
                    opp_immediate_win = has_immediate_winning_move(root.pieces, opp(cpu_player));
                    if (root_risk >= 1000000) {
                        unmake_move_inplace(root, u);
                        continue;
                    }
                }

                int val;
                if (root_move_idx == 0) {
                    val = alphabeta(root, cur_depth-1, window_alpha, window_beta, false,
                                    cpu_player, 1, true, &m, &td);
                } else {
                    // PVS: zero-window search first
                    val = alphabeta(root, cur_depth-1, window_alpha, window_alpha+1, false,
                                    cpu_player, 1, true, &m, &td);
                    if (val > window_alpha && val < window_beta) {
                        val = alphabeta(root, cur_depth-1, window_alpha, window_beta, false,
                                        cpu_player, 1, true, &m, &td);
                    }
                }

                int style_penalty = 0;
                if (opening_phase) {
                    style_penalty += root_risk / (very_early_opening ? 3 : 6);
                    if (opp_immediate_win) style_penalty += 250000;
                    if (very_early_opening && root_risk > base_opening_risk + 4500)
                        style_penalty += 900;
                    if (moved_kind == "Af" && !root_is_capture)
                        style_penalty += very_early_opening ? 280 : 120;
                    if (moved_kind == "Af" && root_risk > base_opening_risk)
                        style_penalty += 180;
                }
                int ranked = val - style_penalty;

                unmake_move_inplace(root, u);
                if (val > cur_best_val) cur_best_val = val;
                if (ranked > cur_best_rank) { cur_best_rank = ranked; cur_best = m; }
                window_alpha = std::max(window_alpha, val);
                root_move_idx++;
                if (val >= window_beta) break;
            }

            if (shared.stop.load(std::memory_order_relaxed)) break;
            if (std::chrono::steady_clock::now() > shared.deadline) break;

            if (cur_best_val <= alpha) { /* fail-low */ }
            else if (cur_best_val >= beta) { /* fail-high */ }
            else { completed = true; break; }

            delta *= 2;
            alpha = std::max(-999999, cur_best_val - delta);
            beta  = std::min( 999999, cur_best_val + delta);
            if (delta > 800) break;
        }

        if (completed) best = cur_best;
        if (completed) prev_score = cur_best_val;

        // Report to shared best if this thread found something good
        if (completed) {
            int global_best = shared.best_score.load(std::memory_order_relaxed);
            if (cur_best_val > global_best || !shared.best_found) {
                std::lock_guard<std::mutex> lk(shared.best_mutex);
                if (cur_best_val > shared.best_score.load(std::memory_order_relaxed) ||
                    !shared.best_found) {
                    shared.best_score.store(cur_best_val, std::memory_order_relaxed);
                    shared.best_move = best;
                    shared.best_found = true;
                }
            }

            // ── Best-move stability tracking (thread 0 only) ────────────
            if (thread_id == 0 && completed) {
                int prev_pid = shared.last_best_pid.load(std::memory_order_relaxed);
                int prev_dc  = shared.last_best_dc.load(std::memory_order_relaxed);
                int prev_dr  = shared.last_best_dr.load(std::memory_order_relaxed);
                if (best.pid == prev_pid && best.dc == prev_dc && best.dr == prev_dr) {
                    shared.best_move_stability.fetch_add(1, std::memory_order_relaxed);
                } else {
                    shared.best_move_stability.store(0, std::memory_order_relaxed);
                }
                shared.last_best_pid.store(best.pid, std::memory_order_relaxed);
                shared.last_best_dc.store(best.dc, std::memory_order_relaxed);
                shared.last_best_dr.store(best.dr, std::memory_order_relaxed);

                // ── Soft-stop: stable best move past soft deadline → stop ────
                int stability = shared.best_move_stability.load(std::memory_order_relaxed);
                if (stability >= 3 && cur_depth >= 4 &&
                    std::chrono::steady_clock::now() > shared.soft_deadline) {
                    shared.stop.store(true, std::memory_order_relaxed);
                }
            }
        }
    }
}

static AIResult smp_cpu_pick_move(const PieceList& pieces, const std::string& cpu_player,
                                   int max_depth, double time_limit_secs,
                                   const std::atomic<bool>* external_stop = nullptr) {
    // Opening book check (single-threaded)
    {
        SearchState root = make_search_state(pieces, cpu_player, cpu_player);
        AllMoves all_moves = all_moves_for(root.pieces, cpu_player);
        if (all_moves.empty()) return {false, {}};
        MoveTriple book_move{};
        if (g_use_opening_book && opening_book_pick(root, cpu_player, book_move)) {
            return {true, book_move};
        }
        // ── Easy move: only one legal move → use minimal time ────────────
        if (all_moves.size() == 1) {
            return {true, all_moves[0]};
        }
    }

    g_tt_age++;
    g_nodes.store(0, std::memory_order_relaxed);

    int num_threads = smp_thread_count();
    if (num_threads < 1) num_threads = 1;

    // ── Dynamic Time Management ──────────────────────────────────────────
    // soft_limit: ideal time (stop if stable), hard_limit: absolute max
    double soft_limit = time_limit_secs * 0.55;
    double hard_limit = time_limit_secs;
    auto search_start = std::chrono::steady_clock::now();

    SMPShared shared;
    shared.deadline = search_start +
                      std::chrono::milliseconds((int)(hard_limit * 1000));
    shared.soft_deadline = search_start +
                           std::chrono::milliseconds((int)(soft_limit * 1000));
    g_deadline = shared.deadline;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &pieces, &cpu_player, max_depth, &shared, external_stop]() {
            g_stop_flag = external_stop;
            g_deadline = shared.deadline;
            reset_time_state();
            smp_worker(i, pieces, cpu_player, max_depth, shared);
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    if (shared.best_found)
        return {true, shared.best_move};

    return {false, {}};
}
