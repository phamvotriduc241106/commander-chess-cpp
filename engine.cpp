/*
 * Commander Chess (Cờ Tư Lệnh) — C++ Port with SDL2
 * ====================================================
 * Player side is selectable in the mode menu (Red or Blue).
 *
 * ── 2026 Strength Upgrade Summary (~+180 Elo target) ──────────────────────
 *  1) Advanced Threat Evaluation (~+80 Elo):
 *     hanging/undefended pressure, cross-domain attack bonuses, carrier
 *     unload threat potential, commander attack amplification, and
 *     win-condition piece targeting.
 *  2) Terrain-Aware Correction History (~+70 Elo):
 *     adds terrain/control/stacking/commander-water context to correction
 *     history for stronger fortress recognition and eval stability.
 *  3) Low-Depth Fortress & Special Draw Recognizer (~+30 Elo):
 *     detects low-depth fortress deadlocks, variant-specific decisive states,
 *     and carrier-loop draw patterns in AB + QSearch.
 *
 * ── Search Engine ───────────────────────────────────────────────────────────
 * Built from scratch for Commander Chess rules; incorporates all major
 * Stockfish 18 search techniques adapted to this game's 12×11 board,
 * 11-piece-type ruleset, and multi-domain terrain (land/sea/sky):
 *
 *  • Lazy SMP — full multi-threaded iterative deepening with shared TT
 *  • PVS (Principal Variation Search) with asymmetric aspiration windows
 *  • LMR (Late Move Reduction) — SF18-tuned formula: ln(d)·ln(m)/2.0
 *  • NMP (Null Move Pruning) with adaptive reduction
 *  • SEE (Static Exchange Evaluation) pruning in both AB and QSearch
 *  • Reverse Futility Pruning extended to depth 4 (SF18)
 *  • Razoring (depths 1–3) + Probcut (depths 5+)
 *  • Singular/Double-Singular/Negative Extensions
 *  • Correction History — SF18 position + material hash correction of
 *    static eval; improves RFP/Razoring/Futility/LMR accuracy
 *  • Node-count time extension — extends search when best move changes
 *    (mirrors SF18's bestMoveChanges time manager)
 *  • In-check quiescence — commander evasions searched in QSearch (SF18)
 *  • IIR (Internal Iterative Reduction)
 *  • Improving heuristic (2-ply eval comparison)
 *  • Transposition Table (two-bucket, age-aware replacement)
 *  • Killer/Countermove/History/Continuation-History heuristics
 *  • MCTS + Alpha-Beta hybrid (AlphaZero-style root)
 *  • Threefold repetition detection in search path
 *  • Opening book with risk assessment
 *
 * Note: Stockfish's NNUE and 64-bit bitboard representation cannot be
 * ported — they are hardcoded for 8×8 standard chess.  All search
 * algorithms above are faithfully re-implemented for Commander Chess.
 *
 * Compile (Linux):
 *   g++ -O2 -o commanderchess commander_chess.cpp \
 *       $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lm -lpthread
 *
 * Compile (macOS with Homebrew):
 *   g++ -O2 -o commanderchess commander_chess.cpp \
 *       $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lm
 *
 * Compile (Windows with MinGW):
 *   g++ -O2 -o commanderchess commander_chess.cpp \
 *       -ISDL2/include -LSDL2/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lm
 *
 * Dependencies: SDL2, SDL2_image, SDL2_ttf
 */

#define SDL_MAIN_HANDLED
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#else
#error "SDL2 headers not found. Install SDL2/SDL2_image/SDL2_ttf and ensure include paths are configured."
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <limits>
#include <new>
#include <random>
#include <set>
#include <sstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(__EMSCRIPTEN__)
#define COMMANDER_ENABLE_THREADS 0
#else
#define COMMANDER_ENABLE_THREADS 1
#endif

// WASM-SAFE: Browser builds run single-threaded by default.
struct EngineNoopMutex {
    void lock() noexcept {}
    void unlock() noexcept {}
};

#if COMMANDER_ENABLE_THREADS
using EngineMutex = std::mutex;
#else
using EngineMutex = EngineNoopMutex;
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(COMMANDER_ENABLE_WEBGPU) && COMMANDER_ENABLE_WEBGPU
#if __has_include(<webgpu/webgpu.h>)
#include <webgpu/webgpu.h>
#define COMMANDER_HAS_WEBGPU_HEADER 1
#else
#define COMMANDER_HAS_WEBGPU_HEADER 0
#endif
#else
#define COMMANDER_HAS_WEBGPU_HEADER 0
#endif

// ═══════════════════════════════════════════════════════════════════════════
// BASE64 DECODER
// ═══════════════════════════════════════════════════════════════════════════

static std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static const std::string b64chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(uint8_t)b64chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}
// ═══════════════════════════════════════════════════════════════════════════
// PIECE IMAGE DATA (loaded from frontend JSON)
// ═══════════════════════════════════════════════════════════════════════════

static std::map<std::string, std::string> PIECE_B64;
static bool g_piece_b64_loaded = false;

static bool load_piece_b64_from_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();
    if (content.empty()) return false;

    // Expected shape:
    // { "sprites": { "C_red":"...", ... } }
    // We only need key/value string pairs.
    static const std::regex pair_re(R"JSON("([A-Za-z_]+)"\s*:\s*"([^"]+)")JSON");
    std::sregex_iterator it(content.begin(), content.end(), pair_re);
    std::sregex_iterator end;

    std::map<std::string, std::string> parsed;
    for (; it != end; ++it) {
        const std::string key = (*it)[1].str();
        const std::string value = (*it)[2].str();
        if (key == "sprites") continue;
        parsed[key] = value;
    }

    if (parsed.empty()) return false;
    PIECE_B64 = std::move(parsed);
    return true;
}

static void ensure_piece_b64_loaded() {
    if (g_piece_b64_loaded) return;
    g_piece_b64_loaded = true;

    const std::array<std::string, 4> candidates = {
        "frontend/public/piece_sprites.json",
        "./frontend/public/piece_sprites.json",
        "../frontend/public/piece_sprites.json",
        "piece_sprites.json"
    };

    for (const auto& path : candidates) {
        if (load_piece_b64_from_json_file(path)) return;
    }

    // Keep PIECE_B64 empty on failure; board rendering will use existing
    // non-texture fallbacks.
}
// ═══════════════════════════════════════════════════════════════════════════
// SOUND ENGINE — synthesized WAV playback via SDL2 audio
// ═══════════════════════════════════════════════════════════════════════════

static const int SAMPLE_RATE = 22050;

struct SoundBuffer {
    std::vector<int16_t> samples;
};

struct SoundNote { float freq, dur, decay; };

static SoundBuffer synth_sound(const std::vector<SoundNote>& notes, float volume = 0.38f) {
    SoundBuffer buf;
    for (auto& n : notes) {
        int cnt = (int)(SAMPLE_RATE * n.dur);
        for (int i = 0; i < cnt; i++) {
            float t = (float)i / SAMPLE_RATE;
            float env = expf(-t * n.decay);
            float v = 0.0f;
            if (n.freq > 0) {
                v = sinf(2*M_PI*n.freq*t)      * 0.7f
                  + sinf(2*M_PI*n.freq*2*t)    * 0.2f
                  + sinf(2*M_PI*n.freq*0.5f*t) * 0.1f;
            }
            int16_t s = (int16_t)std::max(-32768, std::min(32767,
                (int)(volume * env * 32767.0f * v)));
            buf.samples.push_back(s);
        }
    }
    return buf;
}

// Global sound buffers
static std::map<std::string, SoundBuffer> g_sounds;
static SDL_AudioDeviceID g_audio_dev = 0;
static SDL_AudioSpec     g_audio_spec;
static bool              g_audio_ok = false;

// Simple audio callback: mixes a single one-shot sound
struct AudioPlayback {
    const SoundBuffer* buf;
    size_t pos;
};
static std::vector<AudioPlayback> g_playbacks;
static EngineMutex g_audio_mutex;

static void audio_callback(void* /*userdata*/, uint8_t* stream, int len) {
    int16_t* out = (int16_t*)stream;
    int n = len / 2;
    memset(stream, 0, len);

    std::lock_guard<EngineMutex> lk(g_audio_mutex);
    for (auto& pb : g_playbacks) {
        for (int i = 0; i < n && pb.pos < pb.buf->samples.size(); i++) {
            int32_t v = (int32_t)out[i] + pb.buf->samples[pb.pos++];
            out[i] = (int16_t)std::max(-32768, std::min(32767, v));
        }
    }
    // Remove finished
    g_playbacks.erase(std::remove_if(g_playbacks.begin(), g_playbacks.end(),
        [](const AudioPlayback& pb){ return pb.pos >= pb.buf->samples.size(); }),
        g_playbacks.end());
}

static void init_audio() {
    SDL_AudioSpec want{};
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 512;
    want.callback = audio_callback;
    g_audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &g_audio_spec, 0);
    if (g_audio_dev) {
        SDL_PauseAudioDevice(g_audio_dev, 0);
        g_audio_ok = true;
    }

    // Build all sounds
    g_sounds["move"]    = synth_sound({{600,0.05f,25},{400,0.07f,30}},        0.30f);
    g_sounds["capture"] = synth_sound({{220,0.06f,20},{160,0.10f,15},{100,0.08f,18}}, 0.50f);
    g_sounds["hero"]    = synth_sound({{523,0.10f,8},{784,0.18f,6}},          0.45f);
    g_sounds["win"]     = synth_sound({{523,0.12f,4},{659,0.12f,4},{784,0.12f,4},{1047,0.28f,3}}, 0.50f);
    g_sounds["invalid"] = synth_sound({{180,0.05f,30},{120,0.08f,25}},        0.30f);
    g_sounds["cpu"]     = synth_sound({{800,0.04f,40}},                       0.18f);
    g_sounds["boom"]    = synth_sound({{120,0.04f,8},{80,0.08f,5},{50,0.12f,4}},0.55f);
}

static void play_sound(const std::string& name) {
    if (!g_audio_ok) return;
    auto it = g_sounds.find(name);
    if (it == g_sounds.end()) return;
    std::lock_guard<EngineMutex> lk(g_audio_mutex);
    g_playbacks.push_back({&it->second, 0});
}

// ═══════════════════════════════════════════════════════════════════════════
// BOARD CONSTANTS & COLORS
// ═══════════════════════════════════════════════════════════════════════════

static const int COLS     = 11;
static const int ROWS     = 12;
static const int CELL     = 60;
static const int PAD      = 44;
static const int PIECE_R  = 25;
static const int BW       = COLS * CELL + PAD * 2;  // 748
static const int BH       = ROWS * CELL + PAD * 2;  // 808
static const int TITLE_H  = 42;   // gold title banner  "★ COMMANDER CHESS ★"
static const int STATUS_H = 44;   // turn status bar below title
static const int PANEL_W  = 300;  // side panel
static const int WIN_W    = BW + PANEL_W;
static const int WIN_H    = TITLE_H + STATUS_H + BH;

struct Color { uint8_t r,g,b,a; };
static Color C_LAND      = {0xf0,0xe8,0xc0,0xff};
static Color C_SEA       = {0xf0,0xe8,0xc0,0xff};
static Color C_RIVER     = {0x88,0xd0,0xf0,0xff};
static Color C_SEA2      = {0xf0,0xe8,0xc0,0xff};
static Color C_GRID      = {0x8a,0x7a,0x50,0xff};
static Color C_SEL       = {0xff,0xd7,0x00,0xff};
static Color C_MOVE      = {0x44,0xcc,0x66,0xaa};
static Color C_CAPTURE   = {0xff,0x44,0x44,0xff};
static Color C_HERO_RING = {0xff,0xee,0x00,0xff};
static Color C_BG        = {0x0d,0x11,0x17,0xff};  // dark navy background
static Color C_PANEL     = {0x11,0x18,0x22,0xff};  // dark panel background
// UI chrome colors (not used on board/pieces)
static Color C_GREEN     = {0x58,0xc8,0x8c,0xff};  // Duolingo green accent
static Color C_GREEN2    = {0x43,0xb0,0x7a,0xff};  // darker green
static Color C_RED_DOT   = {0xdc,0x35,0x45,0xff};  // red player dot
static Color C_BLUE_DOT  = {0x3b,0x82,0xf6,0xff};  // blue player dot
static Color C_TEXT_DIM  = {0x90,0xa4,0xae,0xff};  // muted text
static Color C_TEXT_BRT  = {0xe8,0xed,0xf2,0xff};  // bright text
static Color C_AMBER     = {0xfb,0xbf,0x24,0xff};  // amber/gold accent

// ═══════════════════════════════════════════════════════════════════════════
// GAME DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

struct PieceDef {
    std::string name;
    int range;
    bool diag;
    bool flies;
    std::string domain; // land/sea/air/commander
};

static std::map<std::string, PieceDef> PIECE_DEF = {
    {"C",  {"Commander",         10, false, false, "commander"}},
    {"H",  {"Headquarters",       0, false, false, "commander"}},
    {"In", {"Infantry",           1, false, false, "land"}},
    {"M",  {"Militia",            1, true,  false, "land"}},
    {"T",  {"Tank",               2, false, false, "land"}},
    {"E",  {"Engineer",           1, false, false, "land"}},
    {"A",  {"Artillery",          3, false, false, "land"}},
    {"Aa", {"Anti-Aircraft",      1, false, false, "land"}},
    {"Ms", {"Missile",            2, false, false, "land"}},
    {"Af", {"Air Force",          4, true,  true,  "air"}},
    {"N",  {"Navy",               4, true,  true,  "sea"}},
};

static std::map<std::string, int> PIECE_VALUE = {
    // Official rulebook point scale ×10 for integer evaluation.
    {"C",1000},{"H",0},{"In",100},{"M",100},
    {"T",200},{"E",100},{"A",300},{"Aa",100},
    {"Ms",200},{"Af",400},{"N",800},
};

// Forward-declared fast helpers (kind_index defined fully later; this early
// version is identical and needed because compute_game_phase / quick_eval
// call piece_value_fast before the main kind_index definition).
static inline int kind_index_early(const std::string& k) {
    if (k.empty()) return 0;
    switch (k[0]) {
        case 'C': return 0;
        case 'H': return 1;
        case 'I': return 2;
        case 'M': return (k.size()>1 && k[1]=='s') ? 8 : 3;
        case 'T': return 4;
        case 'E': return 5;
        case 'A': if (k.size()>1) { if (k[1]=='a') return 7; if (k[1]=='f') return 9; }
                  return 6;
        case 'N': return 10;
        default: return 0;
    }
}
static const int PIECE_VALUE_FAST[11] = {1000, 0, 100, 100, 200, 100, 300, 100, 200, 400, 800};
static inline int piece_value_fast(const std::string& kind) {
    int ki = kind_index_early(kind);
    return (ki >= 0 && ki < 11) ? PIECE_VALUE_FAST[ki] : 0;
}

struct Piece {
    int  id;
    std::string player; // "red" or "blue"
    std::string kind;
    int  col, row;
    bool hero;
    int  carrier_id; // -1 when not carried; otherwise piece id of carrier
};

struct PieceList {
    static constexpr std::size_t kMaxPieces = 132;
    using value_type = Piece;
    using size_type = std::size_t;
    using iterator = Piece*;
    using const_iterator = const Piece*;

    std::array<Piece, kMaxPieces> items{};
    size_type len = 0;

    iterator begin() noexcept { return items.data(); }
    iterator end() noexcept { return items.data() + len; }
    const_iterator begin() const noexcept { return items.data(); }
    const_iterator end() const noexcept { return items.data() + len; }
    const_iterator cbegin() const noexcept { return items.data(); }
    const_iterator cend() const noexcept { return items.data() + len; }

    size_type size() const noexcept { return len; }
    bool empty() const noexcept { return len == 0; }
    void clear() noexcept { len = 0; }

    void reserve(size_type n) {
        if (n > kMaxPieces) throw std::length_error("PieceList reserve overflow");
    }

    value_type& operator[](size_type i) { return items[i]; }
    const value_type& operator[](size_type i) const { return items[i]; }
    value_type& front() { return items[0]; }
    const value_type& front() const { return items[0]; }
    value_type& back() { return items[len - 1]; }
    const value_type& back() const { return items[len - 1]; }

    value_type* data() noexcept { return items.data(); }
    const value_type* data() const noexcept { return items.data(); }

    void push_back(const value_type& v) {
        if (len >= kMaxPieces) throw std::length_error("PieceList push_back overflow");
        items[len++] = v;
    }

    void push_back(value_type&& v) {
        if (len >= kMaxPieces) throw std::length_error("PieceList push_back overflow");
        items[len++] = std::move(v);
    }

    template <typename... Args>
    value_type& emplace_back(Args&&... args) {
        if (len >= kMaxPieces) throw std::length_error("PieceList emplace_back overflow");
        items[len] = value_type{std::forward<Args>(args)...};
        return items[len++];
    }

    void pop_back() noexcept {
        if (len > 0) --len;
    }

    iterator erase(iterator pos) {
        size_type idx = static_cast<size_type>(pos - begin());
        if (idx >= len) return end();
        for (size_type i = idx; i + 1 < len; i++) items[i] = std::move(items[i + 1]);
        --len;
        return begin() + idx;
    }

    iterator erase(iterator first, iterator last) {
        if (first == last) return first;
        size_type idx_first = static_cast<size_type>(first - begin());
        size_type idx_last = static_cast<size_type>(last - begin());
        if (idx_first >= len) return end();
        idx_last = std::min(idx_last, len);
        size_type remove_count = idx_last - idx_first;
        for (size_type i = idx_first; i + remove_count < len; i++) {
            items[i] = std::move(items[i + remove_count]);
        }
        len -= remove_count;
        return begin() + idx_first;
    }

    iterator insert(iterator pos, const value_type& v) {
        size_type idx = static_cast<size_type>(pos - begin());
        if (idx > len) idx = len;
        if (len >= kMaxPieces) throw std::length_error("PieceList insert overflow");
        for (size_type i = len; i > idx; i--) items[i] = std::move(items[i - 1]);
        items[idx] = v;
        ++len;
        return begin() + idx;
    }

    iterator insert(iterator pos, value_type&& v) {
        size_type idx = static_cast<size_type>(pos - begin());
        if (idx > len) idx = len;
        if (len >= kMaxPieces) throw std::length_error("PieceList insert overflow");
        for (size_type i = len; i > idx; i--) items[i] = std::move(items[i - 1]);
        items[idx] = std::move(v);
        ++len;
        return begin() + idx;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// BOARD HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static std::string opp(const std::string& p) { return p=="red" ? "blue" : "red"; }
static bool on_board(int c, int r) { return c>=0 && c<=10 && r>=0 && r<=11; }
static bool is_sea(int c, int /*r*/) { return c <= 2; }
static bool is_reef(int c) { return c==5 || c==7; }
static bool is_navigable(int c, int r) {
    if (!on_board(c, r)) return false;
    if (is_sea(c, r)) return true;
    // Deep-water river segment excluding reef-base columns.
    return (r == 5 || r == 6) && c >= 2 && c <= 10 && !is_reef(c);
}
static bool is_hq_square(int c, int r) {
    return (r == 0 || r == 11) && (c == 4 || c == 6);
}
static bool crosses_river(int r1, int r2) {
    return (r1<=5 && r2>=6) || (r1>=6 && r2<=5);
}

static Piece* piece_at(PieceList& pieces, int col, int row) {
    for (auto& p : pieces)
        if (p.carrier_id < 0 && p.col==col && p.row==row) return &p;
    return nullptr;
}
static const Piece* piece_at_c(const PieceList& pieces, int col, int row) {
    for (auto& p : pieces)
        if (p.carrier_id < 0 && p.col==col && p.row==row) return &p;
    return nullptr;
}

static Piece* piece_by_id(PieceList& pieces, int id) {
    for (auto& p : pieces) if (p.id == id) return &p;
    return nullptr;
}

static const Piece* piece_by_id_c(const PieceList& pieces, int id) {
    for (auto& p : pieces) if (p.id == id) return &p;
    return nullptr;
}

static bool is_person_payload_kind(const std::string& kind) {
    return kind == "In" || kind == "M" || kind == "E" || kind == "C";
}

static bool is_ground_piece_kind(const std::string& kind) {
    return kind == "C" || kind == "H" ||
           kind == "In" || kind == "M" || kind == "T" || kind == "E" ||
           kind == "A" || kind == "Aa" || kind == "Ms";
}

static bool can_carry_kind(const std::string& carrier_kind, const std::string& carried_kind) {
    if (carried_kind == "H") return false;
    if (carried_kind == "C") {
        return carrier_kind == "T" || carrier_kind == "Af" || carrier_kind == "N";
    }
    if (carrier_kind == "N") {
        return carried_kind == "Af" || carried_kind == "T" || is_person_payload_kind(carried_kind);
    }
    if (carrier_kind == "T") {
        // Tank carries one person payload.
        return is_person_payload_kind(carried_kind);
    }
    if (carrier_kind == "Af") {
        // Air force transports one tank OR one person payload.
        return carried_kind == "In" || carried_kind == "M" || carried_kind == "E" ||
               carried_kind == "T";
    }
    if (carrier_kind == "E") {
        // Engineer can ferry heavy vehicles across river segments.
        return carried_kind == "Aa" || carried_kind == "A" || carried_kind == "Ms";
    }
    return false;
}

static bool carrier_capacity_allows_add(const PieceList& pieces, int carrier_id,
                                        const std::string& carrier_kind,
                                        const std::string& add_kind) {
    int af = 0, tank = 0, person = 0, other = 0;
    auto bump = [&](const std::string& kind) {
        if (kind == "Af") af++;
        else if (kind == "T") tank++;
        else if (is_person_payload_kind(kind)) person++;
        else other++;
    };
    for (const auto& p : pieces) {
        if (p.carrier_id == carrier_id) bump(p.kind);
    }
    bump(add_kind);

    if (carrier_kind == "N") {
        if (other > 0) return false;
        // Allowed Navy cargo combos:
        // 2 Af, or 2 T, or 1 Af+1 T, or 1 Af+1 person.
        if (af == 0 && tank <= 2 && person == 0) return true;
        if (tank == 0 && af <= 2 && person == 0) return true;
        if (af == 1 && tank == 1 && person == 0) return true;
        if (af == 1 && tank == 0 && person == 1) return true;
        if (af == 0 && tank == 0 && person <= 1) return true;
        return false;
    }
    if (carrier_kind == "Af") {
        if (af > 0 || other > 0) return false;
        // One tank OR one person.
        return (tank + person) <= 1;
    }
    if (carrier_kind == "T") {
        if (af > 0 || tank > 0 || other > 0) return false;
        // One person.
        return person <= 1;
    }
    return true;
}

static bool carrier_capacity_valid(const PieceList& pieces, int carrier_id, const std::string& carrier_kind) {
    int af = 0, tank = 0, person = 0, other = 0;
    auto bump = [&](const std::string& kind) {
        if (kind == "Af") af++;
        else if (kind == "T") tank++;
        else if (is_person_payload_kind(kind)) person++;
        else other++;
    };
    for (const auto& p : pieces) {
        if (p.carrier_id == carrier_id) bump(p.kind);
    }

    if (carrier_kind == "N") {
        if (other > 0) return false;
        if (af == 0 && tank <= 2 && person == 0) return true;
        if (tank == 0 && af <= 2 && person == 0) return true;
        if (af == 1 && tank == 1 && person == 0) return true;
        if (af == 1 && tank == 0 && person == 1) return true;
        if (af == 0 && tank == 0 && person <= 1) return true;
        return false;
    }
    if (carrier_kind == "Af") {
        if (af > 0 || other > 0) return false;
        return (tank + person) <= 1;
    }
    if (carrier_kind == "T") {
        if (af > 0 || tank > 0 || other > 0) return false;
        return person <= 1;
    }
    return true;
}

static bool can_stack_together(const PieceList& pieces, const Piece& a, const Piece& b) {
    if (a.player != b.player || a.id == b.id) return false;
    if (!can_carry_kind(a.kind, b.kind)) return false;
    return carrier_capacity_allows_add(pieces, a.id, a.kind, b.kind);
}

static bool piece_has_carried_children(const PieceList& pieces, int carrier_id) {
    for (const auto& p : pieces) if (p.carrier_id == carrier_id) return true;
    return false;
}

static void collect_carried_ids(const PieceList& pieces, int carrier_id, std::set<int>& out_ids) {
    for (const auto& p : pieces) {
        if (p.carrier_id != carrier_id) continue;
        if (!out_ids.insert(p.id).second) continue;
        collect_carried_ids(pieces, p.id, out_ids);
    }
}

static void remove_piece_with_carried(PieceList& pieces, int root_id) {
    std::set<int> ids;
    ids.insert(root_id);
    collect_carried_ids(pieces, root_id, ids);
    pieces.erase(std::remove_if(pieces.begin(), pieces.end(),
                                [&](const Piece& p){ return ids.count(p.id) != 0; }),
                 pieces.end());
}

static void sync_carried_positions(PieceList& pieces, int carrier_id) {
    Piece* carrier = piece_by_id(pieces, carrier_id);
    if (!carrier) return;
    for (auto& p : pieces) {
        if (p.carrier_id != carrier_id) continue;
        p.col = carrier->col;
        p.row = carrier->row;
        sync_carried_positions(pieces, p.id);
    }
}

static bool is_carried_by_engineer(const Piece& p, const PieceList& pieces) {
    int cid = p.carrier_id;
    while (cid >= 0) {
        const Piece* c = piece_by_id_c(pieces, cid);
        if (!c) return false;
        if (c->kind == "E") return true;
        cid = c->carrier_id;
    }
    return false;
}

static bool in_aa_range(const PieceList& pieces, int col, int row, const std::string& player) {
    for (auto& p : pieces) {
        if (p.player == player) continue; // only enemy
        int d = std::max(std::abs(p.col-col), std::abs(p.row-row));
        if (p.kind=="Aa" && d<=1) return true;
        if (p.kind=="Ms" && d<=2) return true;
        if (p.kind=="N"  && d<=1) return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// MOVE GENERATION
// ═══════════════════════════════════════════════════════════════════════════

using Move2 = std::pair<int,int>;

static inline int sq_index(int c, int r) { return r * COLS + c; }
static inline int sq_col(int sq) { return sq % COLS; }
static inline int sq_row(int sq) { return sq / COLS; }

struct BB132 {
    uint64_t w[3]{0,0,0};

    inline void clear() { w[0]=w[1]=w[2]=0; }
    inline void set(int sq) {
        w[sq >> 6] |= (1ULL << (sq & 63));
    }
    inline bool test(int sq) const {
        return (w[sq >> 6] & (1ULL << (sq & 63))) != 0;
    }
    inline void or_bits(const BB132& o) {
        w[0] |= o.w[0]; w[1] |= o.w[1]; w[2] |= o.w[2];
    }
};

static int bb_popcount(const BB132& b) {
#if defined(__AVX2__)
    alignas(32) uint64_t lanes[4] = {b.w[0], b.w[1], b.w[2], 0};
    __m256i v = _mm256_load_si256((const __m256i*)lanes);
    const __m256i lut = _mm256_setr_epi8(
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4);
    const __m256i low_mask = _mm256_set1_epi8(0x0f);
    __m256i lo = _mm256_and_si256(v, low_mask);
    __m256i hi = _mm256_and_si256(_mm256_srli_epi16(v, 4), low_mask);
    __m256i pc = _mm256_add_epi8(_mm256_shuffle_epi8(lut, lo),
                                 _mm256_shuffle_epi8(lut, hi));
    __m256i sum64 = _mm256_sad_epu8(pc, _mm256_setzero_si256());
    alignas(32) uint64_t sums[4];
    _mm256_store_si256((__m256i*)sums, sum64);
    return (int)(sums[0] + sums[1] + sums[2] + sums[3]);
#else
    return (int)(__builtin_popcountll(b.w[0]) +
                 __builtin_popcountll(b.w[1]) +
                 __builtin_popcountll(b.w[2]));
#endif
}

static int bb_pop_lsb(BB132& b) {
    for (int wi = 0; wi < 3; wi++) {
        uint64_t v = b.w[wi];
        if (!v) continue;
        int bit = __builtin_ctzll(v);
        b.w[wi] = v & (v - 1);
        return wi * 64 + bit;
    }
    return -1;
}

static std::vector<Move2> bb_to_moves_sorted(const BB132& bb) {
    std::vector<Move2> out;
    out.reserve(bb_popcount(bb));
    // Preserve old set<pair<int,int>> ordering: (col,row).
    for (int c = 0; c < COLS; c++) {
        for (int r = 0; r < ROWS; r++) {
            int sq = sq_index(c, r);
            if (bb.test(sq)) out.push_back({c, r});
        }
    }
    return out;
}

struct MoveGenContext {
    const PieceList* pieces = nullptr;
    std::array<int, COLS * ROWS> sq_to_piece{};
    BB132 occ_all;
    BB132 occ_by_player[2];
    BB132 aa_cover_by_player[2];
    int commander_sq[2]{-1, -1};
};

static inline int player_idx(const std::string& p) { return p=="red" ? 0 : 1; }

static MoveGenContext build_movegen_context(const PieceList& pieces) {
    MoveGenContext ctx;
    ctx.pieces = &pieces;
    ctx.sq_to_piece.fill(-1);

    for (int i = 0; i < (int)pieces.size(); i++) {
        const Piece& p = pieces[i];
        if (!on_board(p.col, p.row)) continue;
        if (p.carrier_id >= 0) continue; // carried pieces do not occupy extra board squares
        int sq = sq_index(p.col, p.row);
        ctx.sq_to_piece[sq] = i;
        ctx.occ_all.set(sq);
        int pl = player_idx(p.player);
        ctx.occ_by_player[pl].set(sq);
        if (p.kind == "C") ctx.commander_sq[pl] = sq;
    }

    // Precompute anti-air coverage for each side (Aa/N range 1, Ms range 2).
    for (const Piece& p : pieces) {
        if (!on_board(p.col, p.row)) continue;
        int radius = 0;
        if (p.kind == "Aa" || p.kind == "N") radius = 1;
        else if (p.kind == "Ms") radius = 2;
        if (radius == 0) continue;
        int pl = player_idx(p.player);
        for (int dc = -radius; dc <= radius; dc++) {
            for (int dr = -radius; dr <= radius; dr++) {
                if (std::max(std::abs(dc), std::abs(dr)) > radius) continue;
                int c = p.col + dc, r = p.row + dr;
                if (!on_board(c, r)) continue;
                ctx.aa_cover_by_player[pl].set(sq_index(c, r));
            }
        }
    }
    return ctx;
}

static BB132 get_move_mask_bitboard(const Piece& piece, const MoveGenContext& ctx) {
    BB132 res;
    const std::string& k = piece.kind;
    int col = piece.col, row = piece.row;
    bool hero = piece.hero;

    if (!on_board(col, row)) return res;
    if (k == "H" && !hero) return res;

    int rng = 0;
    bool use_diag = false;
    if (k == "H") {
        // Heroic headquarters moves like heroic infantry.
        rng = 2;
        use_diag = true;
    } else {
        auto it = PIECE_DEF.find(k);
        if (it == PIECE_DEF.end()) return res;
        rng = it->second.range + (hero ? 1 : 0);
        use_diag = it->second.diag || hero;
    }

    int me = player_idx(piece.player);
    int enemy = 1 - me;

    static const int ORTHO[4][2] = {{0,1},{0,-1},{1,0},{-1,0}};
    static const int DIAG[4][2]  = {{1,1},{1,-1},{-1,1},{-1,-1}};

    auto add_sq = [&](int c, int r) {
        if (piece.kind != "C" && is_hq_square(c, r)) return;
        if (on_board(c, r)) res.set(sq_index(c, r));
    };
    auto piece_index_at = [&](int c, int r) -> int {
        if (!on_board(c, r)) return -1;
        return ctx.sq_to_piece[sq_index(c, r)];
    };
    auto enemy_piece_at = [&](int c, int r) -> const Piece* {
        int pi = piece_index_at(c, r);
        if (pi < 0) return nullptr;
        const Piece& p = (*ctx.pieces)[pi];
        return player_idx(p.player) == me ? nullptr : &p;
    };
    auto can_stack_at = [&](int c, int r) -> bool {
        int pi = piece_index_at(c, r);
        if (pi < 0) return false;
        const Piece& t = (*ctx.pieces)[pi];
        return can_stack_together(*ctx.pieces, piece, t);
    };

    if (k == "C") {
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    const Piece& t = (*ctx.pieces)[pi];
                    if (player_idx(t.player) != me && s == 1) add_sq(nc, nr);
                    break;
                }
                add_sq(nc, nr);
            }
        }

        int enemy_cmd_sq = ctx.commander_sq[enemy];
        if (enemy_cmd_sq < 0) return res;

        int oc = sq_col(enemy_cmd_sq);
        int orow = sq_row(enemy_cmd_sq);
        BB132 filtered;
        BB132 tmp = res;
        while (true) {
            int sq = bb_pop_lsb(tmp);
            if (sq < 0) break;
            int nc = sq_col(sq), nr = sq_row(sq);
            bool exposed = false;
            if (nc == oc) {
                int mn = std::min(nr, orow), mx = std::max(nr, orow);
                bool blocked = false;
                for (int rr = mn + 1; rr < mx; rr++) {
                    int pi = piece_index_at(nc, rr);
                    if (pi >= 0 && (*ctx.pieces)[pi].id != piece.id) { blocked = true; break; }
                }
                if (!blocked) exposed = true;
            } else if (nr == orow) {
                int mn = std::min(nc, oc), mx = std::max(nc, oc);
                bool blocked = false;
                for (int cc = mn + 1; cc < mx; cc++) {
                    int pi = piece_index_at(cc, nr);
                    if (pi >= 0 && (*ctx.pieces)[pi].id != piece.id) { blocked = true; break; }
                }
                if (!blocked) exposed = true;
            }
            if (exposed) filtered.set(sq);
        }
        return filtered;
    }

    if (k == "N") {
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr) || !is_navigable(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    const Piece& t = (*ctx.pieces)[pi];
                    if (player_idx(t.player) != me) {
                        add_sq(nc, nr);
                        break;
                    }
                    if (can_stack_together(*ctx.pieces, piece, t)) add_sq(nc, nr);
                    // Rule: Navy movement is not blocked by friendly pieces.
                    continue;
                }
                add_sq(nc, nr);
            }
        }
        for (auto& d : DIAG) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr) || !is_navigable(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    const Piece& t = (*ctx.pieces)[pi];
                    if (player_idx(t.player) != me) {
                        add_sq(nc, nr);
                        break;
                    }
                    if (can_stack_together(*ctx.pieces, piece, t)) add_sq(nc, nr);
                    continue;
                }
                add_sq(nc, nr);
            }
        }
        // Gunboat fire (ground targets: max 3) and anti-ship missile (enemy Navy: max rng).
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    const Piece& t = (*ctx.pieces)[pi];
                    if (player_idx(t.player) != me) {
                        // Gunboat: ground targets capped at range 3
                        // Anti-ship missile: enemy Navy up to full range
                        if (t.kind == "N" || (is_ground_piece_kind(t.kind) && s <= 3)) {
                            add_sq(nc, nr);
                        }
                    }
                    break;
                }
            }
        }
        return res;
    }

    if (k == "Af") {
        int dirs[8][2] = {{0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}};
        int ndirs = use_diag ? 8 : 4;
        auto path_hits_enemy_aa = [&](int dx, int dy, int steps) -> bool {
            if (hero) return false;
            // Intermediate AA coverage blocks flight before landing.
            for (int t = 1; t < steps; t++) {
                int pc = col + dx * t, pr = row + dy * t;
                if (!on_board(pc, pr)) return true;
                if (ctx.aa_cover_by_player[enemy].test(sq_index(pc, pr))) return true;
            }
            return false;
        };
        for (int di = 0; di < ndirs; di++) {
            int dx = dirs[di][0], dy = dirs[di][1];
            for (int s = 1; s <= rng; s++) {
                int nc = col + dx * s, nr = row + dy * s;
                if (!on_board(nc, nr)) break;
                if (path_hits_enemy_aa(dx, dy, s)) continue;
                int pi = piece_index_at(nc, nr);
                bool dest_in_enemy_aa = !hero && ctx.aa_cover_by_player[enemy].test(sq_index(nc, nr));
                if (is_sea(nc, nr)) {
                    if (pi >= 0) {
                        const Piece& t = (*ctx.pieces)[pi];
                        // Capture landing is legal even inside AA (kamikaze resolved in apply_move).
                        if (player_idx(t.player) != me && t.kind == "N") add_sq(nc, nr);
                        else if (player_idx(t.player) == me &&
                                 can_stack_together(*ctx.pieces, piece, t) &&
                                 !dest_in_enemy_aa) add_sq(nc, nr);
                        // Air force ray stops after first occupied square.
                        break;
                    }
                    continue;
                }
                if (pi >= 0) {
                    const Piece& t = (*ctx.pieces)[pi];
                    // Capture landing is legal even inside AA (kamikaze resolved in apply_move).
                    if (player_idx(t.player) != me) add_sq(nc, nr);
                    else if (can_stack_together(*ctx.pieces, piece, t) && !dest_in_enemy_aa) add_sq(nc, nr);
                    // Air force ray stops after first occupied square.
                    break;
                }
                if (!dest_in_enemy_aa) add_sq(nc, nr);
            }
        }
        return res;
    }

    if (k == "A") {
        int dirs[8][2] = {{0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}};
        int ndirs = use_diag ? 8 : 4;
        bool eng_carried = is_carried_by_engineer(piece, *ctx.pieces);
        for (int di = 0; di < ndirs; di++) {
            int dx = dirs[di][0], dy = dirs[di][1];
            for (int s = 1; s <= rng; s++) {
                int nc = col + dx * s, nr = row + dy * s;
                if (!on_board(nc, nr)) break;
                if (is_sea(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (crosses_river(row, nr) && !is_reef(col) && !eng_carried) {
                    if (pi >= 0 && player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                    break;
                }
                if (pi >= 0) {
                    if (player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                    else if (can_stack_at(nc, nr)) add_sq(nc, nr);
                    break;
                }
                add_sq(nc, nr);
            }
        }
        for (auto& d : ORTHO) {
            for (int s = 1; s <= 3; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    if (player_idx((*ctx.pieces)[pi].player) != me && is_sea(nc, nr)) add_sq(nc, nr);
                    break;
                }
            }
        }
        return res;
    }

    if (k == "Aa") {
        bool eng_carried = is_carried_by_engineer(piece, *ctx.pieces);
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                if (is_sea(nc, nr)) break;
                if (crosses_river(row, nr) && !is_reef(col) && !eng_carried) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    if (player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                    else if (can_stack_at(nc, nr)) add_sq(nc, nr);
                    break;
                }
                add_sq(nc, nr);
            }
        }
        return res;
    }

    if (k == "Ms") {
        bool eng_carried = is_carried_by_engineer(piece, *ctx.pieces);
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                if (is_sea(nc, nr)) break;
                if (crosses_river(row, nr) && !is_reef(col) && !eng_carried) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    if (player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                    else if (can_stack_at(nc, nr)) add_sq(nc, nr);
                    break;
                }
                add_sq(nc, nr);
            }
        }
        // Missile fire ring: orthogonal range 2, diagonal range 1 only.
        for (int dc = -2; dc <= 2; dc++) {
            for (int dr = -2; dr <= 2; dr++) {
                if (dc == 0 && dr == 0) continue;
                // Only pure orthogonal (range 1–2) or pure adjacent diagonal (range 1):
                bool is_orthogonal = (dc == 0 || dr == 0);
                bool is_adj_diag   = (std::abs(dc) == 1 && std::abs(dr) == 1);
                if (!is_orthogonal && !is_adj_diag) continue;
                int nc = col + dc, nr = row + dr;
                if (!on_board(nc, nr)) continue;
                // Bug #4 fix: Missile only hits ground and air targets, not Navy/sea.
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    const Piece& tp = (*ctx.pieces)[pi];
                    if (player_idx(tp.player) != me && tp.kind != "N" && !is_sea(nc, nr))
                        add_sq(nc, nr);
                }
            }
        }
        return res;
    }

    if (k == "E") {
        for (auto& d : ORTHO) {
            int nc = col + d[0], nr = row + d[1];
            if (!on_board(nc, nr) || is_sea(nc, nr)) continue;
            int pi = piece_index_at(nc, nr);
            if (pi >= 0) {
                if (player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                else if (can_stack_at(nc, nr)) add_sq(nc, nr);
            } else {
                add_sq(nc, nr);
            }
        }
        return res;
    }

    // Infantry, Militia, Tank.
    int dirs[8][2] = {{0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1}};
    int ndirs = use_diag ? 8 : 4;
    for (int di = 0; di < ndirs; di++) {
        int dx = dirs[di][0], dy = dirs[di][1];
        for (int s = 1; s <= rng; s++) {
            int nc = col + dx * s, nr = row + dy * s;
            if (!on_board(nc, nr) || is_sea(nc, nr)) break;
            int pi = piece_index_at(nc, nr);
            if (pi >= 0) {
                if (player_idx((*ctx.pieces)[pi].player) != me) add_sq(nc, nr);
                else if (can_stack_at(nc, nr)) add_sq(nc, nr);
                break;
            }
            add_sq(nc, nr);
        }
    }

    // Tank sea-capture: Tank can stand still to capture enemy pieces at sea (range up to 2).
    // This is a fire-only action — the Tank does not move onto the sea square.
    if (k == "T") {
        for (auto& d : ORTHO) {
            for (int s = 1; s <= rng; s++) {
                int nc = col + d[0] * s, nr = row + d[1] * s;
                if (!on_board(nc, nr)) break;
                int pi = piece_index_at(nc, nr);
                if (pi >= 0) {
                    if (is_sea(nc, nr) && player_idx((*ctx.pieces)[pi].player) != me)
                        add_sq(nc, nr);
                    break;
                }
                // If empty land square, continue scanning; if empty sea square, also continue.
            }
        }
    }

    return res;
}

static std::vector<Move2> get_moves_with_ctx(const Piece& piece, const MoveGenContext& ctx) {
    return bb_to_moves_sorted(get_move_mask_bitboard(piece, ctx));
}

static std::vector<Move2> get_moves(const Piece& piece, const PieceList& pieces_in) {
    MoveGenContext ctx = build_movegen_context(pieces_in);
    return get_moves_with_ctx(piece, ctx);
}

static bool has_legal_destination(const Piece& piece, const PieceList& pieces,
                                  int dc, int dr) {
    auto mvs = get_moves(piece, pieces);
    for (const auto& mv : mvs) {
        if (mv.first == dc && mv.second == dr) return true;
    }
    return false;
}

static bool square_capturable_by_player(const PieceList& pieces, int col, int row,
                                        const std::string& by_player) {
    for (const auto& p : pieces) {
        if (p.player != by_player) continue;
        if (!on_board(p.col, p.row)) continue;
        auto mvs = get_moves(p, pieces);
        for (const auto& mv : mvs) {
            if (mv.first == col && mv.second == row) return true;
        }
    }
    return false;
}

static void promote_heroes_from_checks(PieceList& pieces) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& p : pieces) {
            if (p.hero) continue;
            if (!on_board(p.col, p.row)) continue;
            const Piece* enemy_cmd = nullptr;
            for (const auto& q : pieces) {
                if (q.player == opp(p.player) && q.kind == "C") {
                    enemy_cmd = &q;
                    break;
                }
            }
            if (!enemy_cmd) continue;
            auto mvs = get_moves(p, pieces);
            for (const auto& mv : mvs) {
                if (mv.first == enemy_cmd->col && mv.second == enemy_cmd->row) {
                    p.hero = true;
                    changed = true;
                    break;
                }
            }
        }

        // Last-protector rule: if only one non-Commander, non-HQ piece remains, it becomes heroic.
        for (const std::string side : {"red", "blue"}) {
            int remaining_eligible = 0;
            int last_idx = -1;
            for (int i = 0; i < (int)pieces.size(); i++) {
                const Piece& p = pieces[i];
                if (p.player != side || !on_board(p.col, p.row)) continue;
                if (p.kind == "C" || p.kind == "H") continue;
                remaining_eligible++;
                last_idx = i;
                if (remaining_eligible > 1) break;
            }
            if (remaining_eligible == 1 && last_idx >= 0 && !pieces[last_idx].hero) {
                pieces[last_idx].hero = true;
                changed = true;
            }
        }
    }
}

static void push_position_history(std::vector<uint64_t>& history, uint64_t hash) {
    history.push_back(hash);
    const size_t MAX_HIST = 200;
    if (history.size() > MAX_HIST) {
        history.erase(history.begin(), history.begin() + (history.size() - MAX_HIST));
    }
}

static bool is_threefold_repetition(const std::vector<uint64_t>& history, uint64_t hash) {
    int cnt = 0;
    for (uint64_t h : history) {
        if (h == hash && ++cnt >= 3) return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// WIN CHECK
// ═══════════════════════════════════════════════════════════════════════════

enum class GameMode {
    FULL_BATTLE,   // Capture the Commander to win.
    MARINE_BATTLE, // First side to destroy 2 enemy Navies wins.
    AIR_BATTLE,    // First side to destroy 2 enemy Air Forces wins.
    LAND_BATTLE,   // First side to destroy 2 Tanks + 2 Infantry + 2 Artillery wins.
};
static GameMode g_game_mode = GameMode::FULL_BATTLE;

static const char* game_mode_name(GameMode mode) {
    switch (mode) {
    case GameMode::MARINE_BATTLE: return "Marine Battle";
    case GameMode::AIR_BATTLE:    return "Air Battle";
    case GameMode::LAND_BATTLE:   return "Land Battle";
    case GameMode::FULL_BATTLE:
    default: return "Full Battle";
    }
}

static std::string check_win(const PieceList& pieces, const std::string& last) {
    std::string op = opp(last);
    int cnt_C=0, cnt_N=0, cnt_Af=0, cnt_T=0, cnt_In=0, cnt_A=0;
    for (auto& p : pieces) {
        if (p.player != op) continue;
        if (!on_board(p.col, p.row)) continue;
        if (p.kind=="C")  cnt_C++;
        if (p.kind=="N")  cnt_N++;
        if (p.kind=="Af") cnt_Af++;
        if (p.kind=="T")  cnt_T++;
        if (p.kind=="In") cnt_In++;
        if (p.kind=="A")  cnt_A++;
    }
    bool commander_captured = (cnt_C == 0);

    switch (g_game_mode) {
    case GameMode::MARINE_BATTLE:
        if (commander_captured) return last + " wins — Commander captured!";
        if (cnt_N == 0)         return last + " wins — Naval division destroyed!";
        break;
    case GameMode::AIR_BATTLE:
        if (commander_captured) return last + " wins — Commander captured!";
        if (cnt_Af == 0)        return last + " wins — Air Force destroyed!";
        break;
    case GameMode::LAND_BATTLE:
        if (commander_captured) return last + " wins — Commander captured!";
        if (cnt_T == 0 && cnt_In == 0 && cnt_A == 0)
            return last + " wins — Land division destroyed!";
        break;
    case GameMode::FULL_BATTLE:
    default:
        if (commander_captured) return last + " wins — Commander captured!";
        break;
    }
    return "";
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIAL BOARD SETUP
// ═══════════════════════════════════════════════════════════════════════════

static PieceList make_initial_pieces() {
    PieceList all;
    int pid = 0;
    auto add = [&](const std::string& player, const std::string& kind, int col, int row) {
        all.push_back({pid++, player, kind, col, row, false, -1});
    };
    // Official setup (Figure 4 in the rulebook), mirrored by side.
    // RED — bottom half
    add("red","C",  6,0);
    add("red","N",  1,1);
    add("red","Af", 4,1); add("red","H", 5,1); add("red","H", 7,1); add("red","Af", 8,1);
    add("red","A",  3,2); add("red","Ms",6,2); add("red","A", 9,2);
    add("red","N",  2,3); add("red","Aa",4,3); add("red","T", 5,3); add("red","T", 7,3); add("red","Aa",8,3);
    add("red","In",2,4); add("red","E", 3,4); add("red","M", 6,4); add("red","E", 9,4); add("red","In",10,4);

    // BLUE — top half
    add("blue","In",10,7); add("blue","E", 9,7); add("blue","M", 6,7); add("blue","E", 3,7); add("blue","In",2,7);
    add("blue","Aa",8,8); add("blue","T", 7,8); add("blue","T", 5,8); add("blue","Aa",4,8); add("blue","N",2,8);
    add("blue","A", 9,9); add("blue","Ms",6,9); add("blue","A", 3,9);
    add("blue","Af",8,10); add("blue","H",7,10); add("blue","H",5,10); add("blue","Af",4,10); add("blue","N",1,10);
    add("blue","C", 6,11);
    // Note: heroic status is granted only when a piece delivers check during gameplay,
    // not at initial setup. Do NOT call promote_heroes_from_checks here.
    return all;
}

// ═══════════════════════════════════════════════════════════════════════════
// AI ENGINE — Iterative Deepening Alpha-Beta + Quiescence + TT + PST
// ═══════════════════════════════════════════════════════════════════════════

// ── Piece-square tables (Midgame + Endgame pairs) ─────────────────────────
// All tables are from blue's perspective (row 0=bottom, 11=top).
// Red's view is mirrored vertically. Phase-interpolated in board_score.

// Commander: midgame → stay back & safe; endgame → become active
static const int PST_C_MG[12][11] = {
    { 0, 0, 0, 0, 2, 4, 2, 0, 0, 0, 0},
    { 0, 0, 0, 1, 4, 6, 4, 1, 0, 0, 0},
    { 0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    { 0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    { 0, 0, 0, 1, 4, 5, 4, 1, 0, 0, 0},
    { 0, 0, 0, 0, 2, 3, 2, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 0, 0, 0,-2,-2,-2, 0, 0, 0, 0},
    { 0, 0, 0,-2,-4,-5,-4,-2, 0, 0, 0},
    { 0, 0,-2,-4,-6,-8,-6,-4,-2, 0, 0},
    { 0,-2,-4,-6,-8,-10,-8,-6,-4,-2, 0},
    { 0,-4,-6,-8,-10,-12,-10,-8,-6,-4, 0},
};
static const int PST_C_EG[12][11] = {
    { 0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    { 0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    { 0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    { 0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    { 0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    { 0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    { 0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    { 0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    { 0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    { 0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    { 0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    { 0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
};

// Infantry/Militia/Engineer: midgame → hold line; endgame → advance aggressively
static const int PST_In_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0, 0, 0, 2, 3, 3, 3, 2, 0, 0, 0},
    {0, 0, 0, 4, 6, 7, 6, 4, 0, 0, 0},
    {0, 0, 2, 5, 8,10, 8, 5, 2, 0, 0},
    {0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    {0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    {0, 0, 3, 5, 8,10, 8, 5, 3, 0, 0},
    {0, 0, 2, 4, 6, 8, 6, 4, 2, 0, 0},
};
static const int PST_In_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0, 0, 0, 0, 0, 0,0,0,0},
    {0,0,0, 2, 3, 3, 3, 2,0,0,0},
    {0, 0, 2, 4, 6, 7, 6, 4, 2, 0, 0},
    {0, 0, 4, 7,10,12,10, 7, 4, 0, 0},
    {0, 0, 6, 9,14,16,14, 9, 6, 0, 0},
    {0, 0, 8,12,18,22,18,12, 8, 0, 0},
    {0, 0,10,14,20,25,20,14,10, 0, 0},
    {0, 0,12,16,22,28,22,16,12, 0, 0},
    {0, 0,10,14,18,22,18,14,10, 0, 0},
};

// Tank: midgame → center control; endgame → enemy territory
static const int PST_T_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0, 0, 0, 2, 4, 5, 4, 2, 0, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
};
static const int PST_T_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0, 0, 0, 2, 5, 6, 5, 2, 0, 0, 0},
    {0, 0, 2, 5, 8, 9, 8, 5, 2, 0, 0},
    {0, 0, 4, 7,11,13,11, 7, 4, 0, 0},
    {0, 0, 5, 8,14,16,14, 8, 5, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 5, 8,14,16,14, 8, 5, 0, 0},
    {0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
};

// Artillery: midgame → safe back positions with range; endgame → more active
static const int PST_A_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0, 0, 0, 3, 6, 7, 6, 3, 0, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 5, 8, 9, 8, 5, 2, 0, 0},
    {0, 0, 3, 6, 9,10, 9, 6, 3, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 2, 5, 8,10, 8, 5, 2, 0, 0},
    {0, 0, 2, 4, 6, 8, 6, 4, 2, 0, 0},
    {0, 0, 0, 2, 4, 6, 4, 2, 0, 0, 0},
    {0, 0, 0, 0, 2, 4, 2, 0, 0, 0, 0},
};
static const int PST_A_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0, 0, 0, 2, 5, 6, 5, 2, 0, 0, 0},
    {0, 0, 0, 3, 6, 7, 6, 3, 0, 0, 0},
    {0, 0, 2, 4, 8, 9, 8, 4, 2, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    {0, 0, 0, 0, 4, 6, 4, 0, 0, 0, 0},
};

// Air Force: midgame → behind lines or flanks; endgame → aggressive
static const int PST_Af_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,2,4,4,4,2,0,0,0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 4, 6, 9,10, 9, 6, 4, 0, 0},
    {0, 0, 5, 8,12,14,12, 8, 5, 0, 0},
    {0, 0, 6, 9,14,16,14, 9, 6, 0, 0},
    {0, 0, 5, 8,12,14,12, 8, 5, 0, 0},
    {0, 0, 4, 6,10,12,10, 6, 4, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
};
static const int PST_Af_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0, 0, 0, 2, 5, 6, 5, 2, 0, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 8,12,18,22,18,12, 8, 0, 0},
    {0, 0, 8,12,18,22,18,12, 8, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 4, 8,12,14,12, 8, 4, 0, 0},
};

// Navy: sea-based; both phases similar (sea positions matter most)
static const int PST_N_MG[12][11] = {
    {8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {6, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {6, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const int PST_N_EG[12][11] = {
    {10,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 8,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 8,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,12, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,14, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10,10, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

// Missile: midgame → centre/forward; endgame → even more forward
static const int PST_Ms_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0, 0, 0, 2, 5, 6, 5, 2, 0, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 3, 6,10,12,10, 6, 3, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 0, 2, 6, 8, 6, 2, 0, 0, 0},
    {0, 0, 0, 0, 4, 6, 4, 0, 0, 0, 0},
};
static const int PST_Ms_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0, 0, 0, 2, 5, 6, 5, 2, 0, 0, 0},
    {0, 0, 2, 4, 8,10, 8, 4, 2, 0, 0},
    {0, 0, 4, 7,12,14,12, 7, 4, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 6,10,16,18,16,10, 6, 0, 0},
    {0, 0, 4, 8,14,16,14, 8, 4, 0, 0},
    {0, 0, 2, 6,10,12,10, 6, 2, 0, 0},
    {0, 0, 0, 4, 8,10, 8, 4, 0, 0, 0},
};

// Anti-air: midgame → central coverage; endgame → similar
static const int PST_Aa_MG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0,0,0,2,4,5,4,2,0,0,0},
    {0,0,2,4,6,7,6,4,2,0,0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 6, 7, 6, 4, 2, 0, 0},
    {0,0,0,2,4,5,4,2,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
};
static const int PST_Aa_EG[12][11] = {
    {0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0,0,0,2,4,5,4,2,0,0,0},
    {0,0,2,4,6,7,6,4,2,0,0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 7, 8, 7, 4, 2, 0, 0},
    {0, 0, 2, 4, 6, 7, 6, 4, 2, 0, 0},
    {0,0,0,2,4,5,4,2,0,0,0},
    {0,0,0,0,2,2,2,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0},
};

// Game phase: 256 = full midgame, 0 = pure endgame.
// Based on non-Commander, non-HQ material remaining.
static const int PHASE_TOTAL = 2 * (100 + 120 + 300*2 + 80*2 + 350*2 + 200*2 + 250 + 500*2 + 450*2);

static int compute_game_phase(const PieceList& pieces) {
    int mat = 0;
    for (auto& p : pieces) {
        if (p.kind == "C" || p.kind == "H") continue;
        int v = piece_value_fast(p.kind);
        mat += v;
    }
    int phase = (mat * 256 + PHASE_TOTAL / 2) / PHASE_TOTAL;
    if (phase > 256) phase = 256;
    if (phase < 0)   phase = 0;
    return phase;
}

// Interpolate between midgame and endgame PST values.
// phase: 256=midgame, 0=endgame.
static int get_pst_phased(const std::string& kind, const std::string& player,
                          int col, int row, int phase) {
    int r = (player == "blue") ? row : (11 - row);
    if (r<0||r>11||col<0||col>10) return 0;
    int mg = 0, eg = 0;
    if      (kind=="C")                    { mg = PST_C_MG[r][col];  eg = PST_C_EG[r][col]; }
    else if (kind=="In"||kind=="M"||kind=="E") { mg = PST_In_MG[r][col]; eg = PST_In_EG[r][col]; }
    else if (kind=="T")                    { mg = PST_T_MG[r][col];  eg = PST_T_EG[r][col]; }
    else if (kind=="A")                    { mg = PST_A_MG[r][col];  eg = PST_A_EG[r][col]; }
    else if (kind=="Af")                   { mg = PST_Af_MG[r][col]; eg = PST_Af_EG[r][col]; }
    else if (kind=="N")                    { mg = PST_N_MG[r][col];  eg = PST_N_EG[r][col]; }
    else if (kind=="Aa")                   { mg = PST_Aa_MG[r][col]; eg = PST_Aa_EG[r][col]; }
    else if (kind=="Ms")                   { mg = PST_Ms_MG[r][col]; eg = PST_Ms_EG[r][col]; }
    else return 0;
    return (mg * phase + eg * (256 - phase)) / 256;
}

// Legacy wrapper for quick_eval (uses midgame PST as approximation)
static int get_pst(const std::string& kind, const std::string& player, int col, int row) {
    return get_pst_phased(kind, player, col, row, 160); // ~60% midgame default
}

// ── Move application (non-destructive) ────────────────────────────────────
struct MoveTriple { int pid; int dc, dr; };

static inline bool same_move(const MoveTriple& a, const MoveTriple& b) {
    return a.pid==b.pid && a.dc==b.dc && a.dr==b.dr;
}

static inline bool valid_move_hint(const MoveTriple& m) {
    return m.pid >= 0 && on_board(m.dc, m.dr);
}

// === CHANGED === Commander Chess special-rule summary for canonical move application:
// 1) Carrying/stacking: legal friendly stacking updates carrier_id links and syncs passengers.
// 2) Heroic promotion: promote_heroes_from_checks runs after every legal move resolution.
// 3) AF anti-air interception: non-hero AF entering enemy AA range is destroyed.
// 4) AF bombardment return: AF can capture then return to source square if destination stays capturable.
// 5) Navy/Tank stay-and-fire: navy/tank may capture without entering forbidden terrain.

// Internal implementation shared by both checked and unchecked apply_move variants.
static PieceList apply_move_impl(const PieceList& pieces, int piece_id, int dc, int dr, const std::string& player) {
    PieceList np = pieces;
    Piece* piece = piece_by_id(np, piece_id);
    if (!piece || piece->player != player) return np;
    if (!on_board(dc, dr)) return np;

    int src_col = piece->col;
    int src_row = piece->row;
    if (piece->carrier_id >= 0) piece->carrier_id = -1; // split from carrier

    Piece* target = piece_at(np, dc, dr);
    bool navy_stays = (piece->kind=="N" && target && target->player != player && !is_navigable(dc,dr))
                    || (piece->kind=="T" && target && target->player != player && is_sea(dc,dr));

    // Friendly-stack move (load / board)
    if (target && target->player == player) {
        Piece mover_before = *piece;
        Piece target_before = *target;
        if (can_stack_together(np, mover_before, target_before)) {
            // Mover becomes the carrier.
            piece->col = dc;
            piece->row = dr;
            target->carrier_id = piece->id;
            target->col = dc;
            target->row = dr;
            sync_carried_positions(np, target->id);
            sync_carried_positions(np, piece->id);
        } else {
            return pieces;
        }
        promote_heroes_from_checks(np);
        return np;
    }

    if (target && target->player != player) {
        Piece captured_before = *target;
        remove_piece_with_carried(np, target->id);
        piece = piece_by_id(np, piece_id);
        if (!piece) return np;

        // Non-hero AF entering enemy AA ring is shot down.
        if (piece->kind=="Af" && !piece->hero && in_aa_range(np, dc, dr, player)) {
            remove_piece_with_carried(np, piece->id);
            promote_heroes_from_checks(np);
            return np;
        }

        if (!navy_stays) {
            piece->col = dc;
            piece->row = dr;
            sync_carried_positions(np, piece->id);
        }

        // Bombardment return-to-base: after a land capture (not Navy, not Aircraft), AF may return if landing is unsafe.
        if (piece->kind == "Af" && captured_before.kind != "N" && captured_before.kind != "Af" && !navy_stays) {
            if (square_capturable_by_player(np, dc, dr, opp(player))) {
                piece->col = src_col;
                piece->row = src_row;
                sync_carried_positions(np, piece->id);
            }
        }
        promote_heroes_from_checks(np);
        return np;
    }

    piece->col = dc;
    piece->row = dr;
    sync_carried_positions(np, piece->id);
    promote_heroes_from_checks(np);
    return np;
}

// Checked version: verifies legality before applying. Used by GUI and opening book.
static PieceList apply_move(const PieceList& pieces, int piece_id, int dc, int dr, const std::string& player) {
    const Piece* piece = piece_by_id_c(pieces, piece_id);
    if (!piece || piece->player != player) return pieces;
    if (!on_board(dc, dr)) return pieces;
    if (!has_legal_destination(*piece, pieces, dc, dr)) return pieces;
    return apply_move_impl(pieces, piece_id, dc, dr, player);
}

// Unchecked version: skips redundant legality check. Used by search when legality
// was already verified by the caller (saves a full move generation per search node).
static PieceList apply_move_unchecked(const PieceList& pieces, int piece_id, int dc, int dr, const std::string& player) {
    return apply_move_impl(pieces, piece_id, dc, dr, player);
}

using AllMoves = std::vector<MoveTriple>;

static AllMoves all_moves_for(const PieceList& pieces, const std::string& player) {
    AllMoves result;
    MoveGenContext ctx = build_movegen_context(pieces);
    for (auto& p : pieces) {
        if (p.player != player) continue;
        auto mvs = get_moves_with_ctx(p, ctx);
        for (auto& m : mvs) result.push_back({p.id, m.first, m.second});
    }
    return result;
}

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
    // Always true in current implementation.
    bool used_snapshot = true;
    PieceList snapshot_pieces;
    Piece moved_piece{};
    Piece captured_piece{};
    bool had_capture = false;
    std::string turn_before;
    uint64_t hash_before = 0;
    int quick_eval_before = 0;
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

// === CHANGED ===
// WASM-SAFE: avoid rebuilding attack cache unless state hash changed.
static inline void ensure_attack_cache(SearchState& st) {
    if (!st.atk.valid || st.atk.key != st.hash) build_attack_cache(st);
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

// === CHANGED ===
struct EngineConfig {
    bool use_mcts = false;
    bool use_opening_book = true;
    std::size_t tt_size_mb = 512;
    int max_depth = 8;
    int time_limit_ms = 3000;
    int mcts_ab_depth = 3;
    bool force_single_thread = false; // WASM-SAFE: true in browser builds.
};

static EngineConfig default_engine_config() {
    EngineConfig cfg;
#if defined(__EMSCRIPTEN__)
    cfg.use_mcts = false;
    cfg.use_opening_book = true;
    cfg.tt_size_mb = 128;
    cfg.max_depth = 8;
    cfg.time_limit_ms = 3000;
    cfg.mcts_ab_depth = 2;
    cfg.force_single_thread = true;
#endif
    return cfg;
}

static EngineConfig g_engine_config = default_engine_config();
static bool& g_use_mcts = g_engine_config.use_mcts;
static bool& g_use_opening_book = g_engine_config.use_opening_book;

static inline int engine_mcts_ab_depth() {
    return std::max(1, g_engine_config.mcts_ab_depth);
}

static void set_engine_config(const EngineConfig& cfg) {
    g_engine_config = cfg;
    if (g_engine_config.tt_size_mb < 8) g_engine_config.tt_size_mb = 8;
#if defined(__EMSCRIPTEN__)
    g_engine_config.force_single_thread = true;
    if (g_engine_config.tt_size_mb > 128) g_engine_config.tt_size_mb = 128;
#endif
}

static const EngineConfig& get_engine_config() {
    return g_engine_config;
}

// Contiguous TT arena with configurable size
static size_t    g_tt_count = 0;      // number of clusters
static size_t    g_tt_mask  = 0;      // count - 1 (power of 2)
static TTCluster* g_TT      = nullptr;
static uint8_t   g_tt_age   = 0;
static void*     g_tt_arena = nullptr;
static size_t    g_tt_arena_bytes = 0;
static constexpr std::align_val_t TT_ARENA_ALIGN = std::align_val_t(64);
// Thread-safe TT striping for SMP probe/store.
static constexpr size_t TT_LOCK_STRIPES = 1024; // power-of-two
static std::array<EngineMutex, TT_LOCK_STRIPES> g_tt_locks;

static inline bool tt_locking_enabled() {
#if COMMANDER_ENABLE_THREADS
    return !get_engine_config().force_single_thread;
#else
    return false;
#endif
}

static inline EngineMutex& tt_lock_for_hash(uint64_t h) {
    return g_tt_locks[h & (TT_LOCK_STRIPES - 1)];
}

static void tt_arena_release() {
    if (!g_tt_arena) return;
    // WASM-SAFE: arena uses malloc + explicit alignment metadata.
    void* raw = reinterpret_cast<void**>(g_tt_arena)[-1];
    std::free(raw);
    g_tt_arena = nullptr;
    g_tt_arena_bytes = 0;
}

static void* tt_arena_alloc(size_t bytes) {
    if (bytes == 0) throw std::bad_alloc();
    const size_t align = static_cast<size_t>(TT_ARENA_ALIGN);
    const size_t total = bytes + align + sizeof(void*);
    void* raw = std::malloc(total);
    if (!raw) throw std::bad_alloc();

    void* aligned_base = static_cast<char*>(raw) + sizeof(void*);
    size_t space = total - sizeof(void*);
    void* aligned = std::align(align, bytes, aligned_base, space);
    if (!aligned) {
        std::free(raw);
        throw std::bad_alloc();
    }

    reinterpret_cast<void**>(aligned)[-1] = raw;
    return aligned;
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
    const size_t preferred_mb = std::max<std::size_t>(8, get_engine_config().tt_size_mb);
    if (preferred_mb > 0) {
        try { tt_resize(preferred_mb); return; } catch (...) {}
    }
#if defined(__EMSCRIPTEN__)
    for (size_t mb : {128, 96, 64, 48, 32, 16, 8}) {
#else
    for (size_t mb : {2048, 1024, 768, 512, 384, 256, 192, 128, 96, 64, 32, 8}) {
#endif
        if (mb == preferred_mb) continue;
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

static inline uint64_t zobrist_cpu_perspective_salt(const std::string& cpu_player) {
    // Keep TT keys disjoint across root perspective in self-play (red-search vs blue-search).
    return (cpu_player == "red") ? 0x9E3779B97F4A7C15ULL : 0ULL;
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
    st.hash = zobrist_hash(st.pieces, st.turn) ^ zobrist_cpu_perspective_salt(cpu_player);
    st.quick_eval = quick_eval_cpu(st.pieces, cpu_player);
    st.atk.valid = false;
    st.rebuild_caches();
    return st;
}

static void debug_validate_state_or_abort(const PieceList& pieces,
                                          const std::string& last_mover,
                                          const char* where) {
#ifdef DEBUG
    std::string reason;
    if (!validate_state_for_sim(pieces, last_mover, &reason)) {
        std::cerr << "[DEBUG] " << where << " produced invalid state: " << reason << "\n";
        std::abort();
    }
#else
    (void)pieces;
    (void)last_mover;
    (void)where;
#endif
}

// === CHANGED ===
static bool make_move_inplace_snapshot(SearchState& st, const MoveTriple& m,
                                       const std::string& cpu_player, UndoMove& u) {
    u = UndoMove{};
    if (!on_board(m.dc, m.dr)) return false;
    int moved_idx = find_piece_idx_by_id(st.pieces, m.pid);
    if (moved_idx < 0) return false;
    if (st.pieces[moved_idx].player != st.turn) return false;
    if (!has_legal_destination(st.pieces[moved_idx], st.pieces, m.dc, m.dr)) return false;

    u.snapshot_pieces = st.pieces;
    u.turn_before = st.turn;
    u.hash_before = st.hash;
    u.quick_eval_before = st.quick_eval;
    u.moved_piece = st.pieces[moved_idx];

    int captured_idx = find_piece_idx_at(st.pieces, m.dc, m.dr);
    if (captured_idx >= 0 && st.pieces[captured_idx].player != st.turn) {
        u.had_capture = true;
        u.captured_piece = st.pieces[captured_idx];
    }

    st.pieces = apply_move_unchecked(st.pieces, m.pid, m.dc, m.dr, st.turn);
    st.turn = opp(st.turn);
    st.hash = zobrist_hash(st.pieces, st.turn) ^ zobrist_cpu_perspective_salt(cpu_player);
    st.quick_eval = quick_eval_cpu(st.pieces, cpu_player);
    st.atk.valid = false;
    st.rebuild_caches();
    debug_validate_state_or_abort(st.pieces, u.turn_before, "make_move_inplace");
    return true;
}

/*
 * make_move_inplace safety policy (WASM-SAFE):
 *  1) Carrying / stacking transitions (including nested passenger sync)
 *  2) Heroic promotions after decisive checks
 *  3) AF anti-air interception on non-hero entry
 *  4) AF bombardment return-to-origin when landing is unsafe
 *  5) Navy/Tank stay-and-fire behavior on non-enterable sea squares
 *
 * These rules are all handled by apply_move(). We always snapshot and replay
 * that canonical path to avoid incremental state-corruption bugs.
 */
static bool make_move_inplace(SearchState& st, const MoveTriple& m,
                              const std::string& cpu_player, UndoMove& u) {
    return make_move_inplace_snapshot(st, m, cpu_player, u);

#if 0
    // Future optimization:
    // Re-introduce an incremental in-place move path once it is formally
    // proven equivalent to snapshot replay for all special Commander rules.
#endif
}

/*
 * unmake_move_inplace safety policy (perfect inverse of make):
 *  1) Carrying / stacking links are restored exactly from snapshot.
 *  2) Heroic promotions are reversed by snapshot restore.
 *  3) AF anti-air interception outcomes are restored exactly.
 *  4) AF bombardment return-to-origin outcomes are restored exactly.
 *  5) Navy/Tank stay-and-fire outcomes are restored exactly.
 *
 * Full snapshot restore (pieces + turn + hash + eval) guarantees no drift.
 */
static void unmake_move_inplace(SearchState& st, const UndoMove& u) {
    st.turn = u.turn_before;
    st.hash = u.hash_before;
    st.quick_eval = u.quick_eval_before;
    st.pieces = u.snapshot_pieces;
    st.atk.valid = false;
    st.rebuild_caches();
    debug_validate_state_or_abort(st.pieces, opp(st.turn), "unmake_move_inplace");
}

// === CHANGED ===
// Move-generator regression helper.
static uint64_t perft_impl(SearchState& st, int depth, const std::string& cpu_player) {
    if (depth <= 0) return 1ULL;
    AllMoves moves = all_moves_for(st.pieces, st.turn);
    if (depth == 1) return (uint64_t)moves.size();
    uint64_t nodes = 0;
    for (const auto& m : moves) {
        UndoMove u;
        if (!make_move_inplace(st, m, cpu_player, u)) continue;
        nodes += perft_impl(st, depth - 1, cpu_player);
        unmake_move_inplace(st, u);
    }
    return nodes;
}

static uint64_t perft(const PieceList& pieces, const std::string& turn, int depth) {
    SearchState st = make_search_state(pieces, turn, turn);
    return perft_impl(st, depth, turn);
}

static uint64_t perft(int depth) {
    PieceList init = make_initial_pieces();
    return perft(init, "red", depth);
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

// ── Correction History forward declarations (full implementation below SEE) ─
static const int CORR_HIST_SIZE     = 16384;
static const int CORR_MAT_SIZE      = 512;
static const int CORR_TERR_SIZE     = 2048;
static int g_corr_hist[2][CORR_HIST_SIZE] = {};
static int g_corr_hist_mat[2][CORR_MAT_SIZE] = {};
static int g_corr_hist_terrain[2][CORR_TERR_SIZE] = {};
static EngineMutex g_corr_hist_mutex;
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
    // Correction history is soft-reset: halve values so prior game info fades
    // but strong patterns carry over (same approach as Stockfish inter-game).
    {
        std::lock_guard<EngineMutex> lk(g_corr_hist_mutex);
        for (int pi = 0; pi < 2; pi++) {
            for (int i = 0; i < CORR_HIST_SIZE; i++) g_corr_hist[pi][i] /= 2;
            for (int i = 0; i < CORR_MAT_SIZE;  i++) g_corr_hist_mat[pi][i] /= 2;
            for (int i = 0; i < CORR_TERR_SIZE; i++) g_corr_hist_terrain[pi][i] /= 2;
        }
    }
    g_default_td.reset();
}

// ── TT probe / store ──────────────────────────────────────────────────────
static const TTEntry* tt_probe(uint64_t h) {
    if (!g_TT) return nullptr;
    auto probe_impl = [&]() -> const TTEntry* {
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
    };
    if (tt_locking_enabled()) {
        std::lock_guard<EngineMutex> lk(tt_lock_for_hash(h));
        return probe_impl();
    }
    return probe_impl();
}

static void tt_store(uint64_t h, int depth, int flag, int val, MoveTriple best) {
    if (!g_TT) return;
    auto store_impl = [&]() {
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
    };
    if (tt_locking_enabled()) {
        std::lock_guard<EngineMutex> lk(tt_lock_for_hash(h));
        store_impl();
        return;
    }
    store_impl();
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
    int bonus = std::min(depth * depth, 1600);
    v += bonus - v * std::abs(bonus) / 32000;
    v = std::max(-32000, std::min(32000, v));
}

static void td_penalise_history(ThreadData& td, int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = td.history[pl][ki][dc][dr];
    int malus = -std::min(depth * depth, 1600);
    v += malus - v * std::abs(malus) / 32000;
    v = std::max(-32000, std::min(32000, v));
}

static int td_cont_history_score(const ThreadData& td, const MoveTriple* prev, int ki, int dc, int dr) {
    if (!prev || !on_board(prev->dc, prev->dr)) return 0;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return (int)td.cont_history[prev->dc][prev->dr][ki][dc][dr];
}

static void td_update_cont_history(ThreadData& td, const MoveTriple* prev, int ki, int dc, int dr, int depth) {
    if (!prev || !on_board(prev->dc, prev->dr)) return;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int bonus = std::min(depth * depth, 1600);
    int v = (int)td.cont_history[prev->dc][prev->dr][ki][dc][dr];
    v += bonus - v * std::abs(bonus) / 32000;
    td.cont_history[prev->dc][prev->dr][ki][dc][dr] = (int16_t)std::max(-32000, std::min(32000, v));
}

static int history_score(int pl, int ki, int dc, int dr) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return g_history[pl][ki][dc][dr];
}

static void update_history(int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = g_history[pl][ki][dc][dr];
    // Stockfish 18 gravity: bonus - entry * |bonus| / MAX_HISTORY
    // This naturally decays old values and prevents saturation.
    int bonus = std::min(depth * depth, 1600);
    v += bonus - v * std::abs(bonus) / 32000;
    v = std::max(-32000, std::min(32000, v));
}

static void penalise_history(int pl, int ki, int dc, int dr, int depth) {
    if (pl<0||pl>=H_PLAYERS||ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int& v = g_history[pl][ki][dc][dr];
    int malus = -std::min(depth * depth, 1600);
    v += malus - v * std::abs(malus) / 32000;
    v = std::max(-32000, std::min(32000, v));
}

static int cont_history_score(const MoveTriple* prev, int ki, int dc, int dr) {
    if (!prev || !on_board(prev->dc, prev->dr)) return 0;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return 0;
    return (int)g_cont_history[prev->dc][prev->dr][ki][dc][dr];
}

static void update_cont_history(const MoveTriple* prev, int ki, int dc, int dr, int depth) {
    if (!prev || !on_board(prev->dc, prev->dr)) return;
    if (ki<0||ki>=H_KINDS||dc<0||dc>=H_COLS||dr<0||dr>=H_ROWS) return;
    int bonus = std::min(depth * depth, 1600);
    int v = (int)g_cont_history[prev->dc][prev->dr][ki][dc][dr];
    v += bonus - v * std::abs(bonus) / 32000;
    g_cont_history[prev->dc][prev->dr][ki][dc][dr] = (int16_t)std::max(-32000, std::min(32000, v));
}

// ═══════════════════════════════════════════════════════════════════════════
// CORRECTION HISTORY  (Stockfish 18 technique)
// ═══════════════════════════════════════════════════════════════════════════
//
// Tracks the signed difference between the static evaluation and the final
// alpha-beta search score, indexed by:
//   (a) a position-hash bucket   — captures transient positional features
//   (b) a material-count bucket  — captures stable material imbalances
//
// These correction values are blended into the static eval before every node
// is pruned, giving the engine a cheap but meaningful bias toward positions
// where history says the static eval under/over-estimates the true score.
// This is the same principle used in Stockfish 16-18 and directly improves
// RFP, Razoring, Probcut, Futility, and LMR accuracy.
// ─────────────────────────────────────────────────────────────────────────

// (CORR_HIST_SIZE, CORR_MAT_SIZE, CORR_TERR_SIZE and corr tables declared above)
static const int CORR_MAX_VAL       = 32000;
static const int CORR_WEIGHT_DENOM  = 256;    // fixed-point denominator

// Material key: sum weighted piece values, bucketed.
static int material_corr_key(const PieceList& pieces, int pi) {
    int key = 0;
    for (const auto& p : pieces) {
        if (p.kind == "H") continue;
        int side = (p.player == "red") ? 0 : 1;
        int sign = (side == pi) ? 1 : -1;
        key += sign * piece_value_fast(p.kind) / 50;
    }
    return (((key % CORR_MAT_SIZE) + CORR_MAT_SIZE) % CORR_MAT_SIZE);
}

static bool commander_near_water_square(int c, int r) {
    if (!on_board(c, r)) return false;
    if (is_sea(c, r) || r == 5 || r == 6) return true;
    for (int dc = -1; dc <= 1; dc++) for (int dr = -1; dr <= 1; dr++) {
        int nc = c + dc, nr = r + dr;
        if (!on_board(nc, nr)) continue;
        if (is_sea(nc, nr) || nr == 5 || nr == 6) return true;
    }
    return false;
}

// === NEW: Terrain-Aware Correction History (~+70 Elo) ===
// Terrain/control/stacking key for fortress-like structure recognition.
static int terrain_corr_key(const PieceList& pieces, int pi) {
    int sea_occ[2] = {0, 0};
    int river_occ[2] = {0, 0};
    int sky_control[2] = {0, 0};
    int cmd_col[2] = {-1, -1};
    int cmd_row[2] = {-1, -1};
    int cmd_exposure[2] = {0, 0};
    int cmd_stack_density[2] = {0, 0};
    int navy_near_water_cmd[2] = {0, 0};

    for (const auto& p : pieces) {
        int s = player_idx(p.player);
        if (!on_board(p.col, p.row)) continue;
        if (is_sea(p.col, p.row)) sea_occ[s] += (p.kind == "N") ? 2 : 1;
        if (p.row == 5 || p.row == 6) river_occ[s] += (p.kind == "E") ? 2 : 1;
        if (p.kind == "Af") sky_control[s] += 3;
        else if (p.kind == "Aa" || p.kind == "Ms") sky_control[s] += 2;
        else if (p.kind == "N") sky_control[s] += 1;
        if (p.kind == "C") { cmd_col[s] = p.col; cmd_row[s] = p.row; }
    }

    for (const auto& p : pieces) {
        int s = player_idx(p.player);
        if (p.kind == "N" && cmd_col[s] >= 0 && commander_near_water_square(cmd_col[s], cmd_row[s])) {
            int dist = std::abs(p.col - cmd_col[s]) + std::abs(p.row - cmd_row[s]);
            if (dist <= 4) navy_near_water_cmd[s] += (5 - dist);
        }
        if (p.carrier_id >= 0 && cmd_col[s] >= 0) {
            int cheb = std::max(std::abs(p.col - cmd_col[s]), std::abs(p.row - cmd_row[s]));
            if (cheb <= 2) cmd_stack_density[s] += 2;
            else if (cheb <= 4) cmd_stack_density[s] += 1;
        }
    }

    for (int s = 0; s < 2; s++) {
        if (cmd_col[s] < 0) continue;
        int c = cmd_col[s], r = cmd_row[s];
        int enemy_touch = 0, friendly_touch = 0, open_touch = 0;
        for (int dc = -1; dc <= 1; dc++) for (int dr = -1; dr <= 1; dr++) {
            if (dc == 0 && dr == 0) continue;
            int nc = c + dc, nr = r + dr;
            if (!on_board(nc, nr)) continue;
            const Piece* occ = piece_at_c(pieces, nc, nr);
            if (!occ) open_touch++;
            else if (player_idx(occ->player) == s) friendly_touch++;
            else enemy_touch++;
        }
        cmd_exposure[s] = enemy_touch * 3 + open_touch - friendly_touch;
        if (commander_near_water_square(c, r)) cmd_exposure[s] += 2;
    }

    auto diff = [&](const int arr[2]) { return arr[pi] - arr[1 - pi]; };
    int d_sea = diff(sea_occ);
    int d_river = diff(river_occ);
    int d_sky = diff(sky_control);
    int d_exposure = diff(cmd_exposure);
    int d_stack = diff(cmd_stack_density);
    int d_navy = diff(navy_near_water_cmd);

    uint64_t mix = 0x9E3779B97F4A7C15ULL;
    auto fold = [&](int v) {
        uint64_t x = (uint64_t)(v + 512);
        mix ^= x + 0x9E3779B97F4A7C15ULL + (mix << 6) + (mix >> 2);
    };
    fold(d_sea);
    fold(d_river);
    fold(d_sky);
    fold(d_exposure);
    fold(d_stack);
    fold(d_navy);
    return (int)(mix & (CORR_TERR_SIZE - 1));
}

static void update_correction_history(uint64_t hash, const PieceList& pieces,
                                      const std::string& player, int depth,
                                      int search_val, int raw_static_eval) {
    // Don't correct near-mate scores — they're exact.
    if (std::abs(search_val) >= 20000 || std::abs(raw_static_eval) >= 20000) return;
    int pi = player_idx(player);
    if (pi < 0) return;

    int diff = search_val - raw_static_eval;
    diff = std::max(-2000, std::min(2000, diff));  // cap individual sample
    int scale = std::max(1, std::min(depth, 16));

    // Hash/material/terrain bucket updates — synchronized for SMP safety.
    int hk = (int)((hash >> 4) & (CORR_HIST_SIZE - 1));
    int mk = material_corr_key(pieces, pi);
    int tk = terrain_corr_key(pieces, pi);
    {
        std::lock_guard<EngineMutex> lk(g_corr_hist_mutex);
        {
            int& e = g_corr_hist[pi][hk];
            e = (e * (CORR_WEIGHT_DENOM - scale) + diff * scale * CORR_WEIGHT_DENOM)
                / CORR_WEIGHT_DENOM;
            e = std::max(-CORR_MAX_VAL, std::min(CORR_MAX_VAL, e));
        }
        {
            int& e = g_corr_hist_mat[pi][mk];
            e = (e * (CORR_WEIGHT_DENOM - scale) + diff * scale * CORR_WEIGHT_DENOM)
                / CORR_WEIGHT_DENOM;
            e = std::max(-CORR_MAX_VAL, std::min(CORR_MAX_VAL, e));
        }
        {
            int& e = g_corr_hist_terrain[pi][tk];
            e = (e * (CORR_WEIGHT_DENOM - scale) + diff * scale * CORR_WEIGHT_DENOM)
                / CORR_WEIGHT_DENOM;
            e = std::max(-CORR_MAX_VAL, std::min(CORR_MAX_VAL, e));
        }
    }
}

// Returns the corrected static eval (raw + blended correction offset).
// The blend weight (0.50 hash + 0.30 material + 0.20 terrain/context) is
// intentionally conservative to stabilize pruning without distorting eval shape.
// enough to improve pruning without distorting the eval surface.
static int corrected_static_eval(uint64_t hash, const PieceList& pieces,
                                 const std::string& player, int raw_eval) {
    int pi = player_idx(player);
    if (pi < 0) return raw_eval;
    int hk = (int)((hash >> 4) & (CORR_HIST_SIZE - 1));
    int mk = material_corr_key(pieces, pi);
    int tk = terrain_corr_key(pieces, pi);
    int hash_corr = 0, mat_corr = 0, terr_corr = 0;
    {
        std::lock_guard<EngineMutex> lk(g_corr_hist_mutex);
        hash_corr = g_corr_hist[pi][hk] / CORR_WEIGHT_DENOM;
        mat_corr  = g_corr_hist_mat[pi][mk] / CORR_WEIGHT_DENOM;
        terr_corr = g_corr_hist_terrain[pi][tk] / CORR_WEIGHT_DENOM;
    }
    // Blend: 50% hash, 30% material, 20% terrain/context.
    int correction = (hash_corr * 5 + mat_corr * 3 + terr_corr * 2) / 10;
    correction = std::max(-180, std::min(180, correction));  // safety clamp
    return raw_eval + correction;
}

// ── Static Exchange Evaluation (SEE) ─────────────────────────────────────
// Returns the material gain/loss of a capture sequence on (col,row).
// Positive = winning capture, negative = losing capture.
static int see(const PieceList& pieces, int col, int row,
               const std::string& attacker_player, int depth=0) {
    if (depth > 6) return 0;
    // Build context once for all pieces in this SEE node.
    MoveGenContext ctx = build_movegen_context(pieces);
    int target_sq = sq_index(col, row);
    // Find least-valuable attacker for attacker_player
    const Piece* best_atk = nullptr;
    int best_val = 999999;
    for (auto& p : pieces) {
        if (p.player != attacker_player) continue;
        BB132 attacks = get_move_mask_bitboard(p, ctx);
        if (attacks.test(target_sq)) {
            int v = std::max(1, piece_value_fast(p.kind));
            if (v < best_val) { best_val=v; best_atk=&p; }
        }
    }
    if (!best_atk) return 0;

    const Piece* target = piece_at_c(pieces, col, row);
    int gain = target ? piece_value_fast(target->kind) : 0;

    // SF18-style early exit: capturing with LVA and still ahead => prune
    if (depth >= 2 && gain - best_val > 0) return gain - best_val;

    PieceList np = apply_move_unchecked(pieces, best_atk->id, col, row, attacker_player);
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
    const int target_sq = sq_index(col, row);
    MoveGenContext ctx = build_movegen_context(pieces);
    int attackers = 0;
    for (auto& p : pieces) {
        if (p.player != attacker_player) continue;
        BB132 attacks = get_move_mask_bitboard(p, ctx);
        if (attacks.test(target_sq)) attackers++;
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
    ensure_attack_cache(st);
    return attackers_to_square(st.pieces, cc, cr, opp(player), &st.atk);
}

struct ObjectiveCounts {
    int commander = 0;
    int navy = 0;
    int air_force = 0;
    int tank = 0;
    int infantry = 0;
    int artillery = 0;
    int active_non_hq = 0;
    int carried_units = 0;
};

static ObjectiveCounts collect_objective_counts(const PieceList& pieces, const std::string& side) {
    ObjectiveCounts out;
    for (const auto& p : pieces) {
        if (p.player != side || !on_board(p.col, p.row)) continue;
        if (p.kind != "H") out.active_non_hq++;
        if (p.carrier_id >= 0) out.carried_units++;
        if (p.kind == "C") out.commander++;
        else if (p.kind == "N") out.navy++;
        else if (p.kind == "Af") out.air_force++;
        else if (p.kind == "T") out.tank++;
        else if (p.kind == "In") out.infantry++;
        else if (p.kind == "A") out.artillery++;
    }
    return out;
}

static bool side_fulfills_win_objective(const ObjectiveCounts& self,
                                        const ObjectiveCounts& enemy) {
    (void)self;
    switch (g_game_mode) {
    case GameMode::MARINE_BATTLE:
        return enemy.commander == 0 || enemy.navy == 0;
    case GameMode::AIR_BATTLE:
        return enemy.commander == 0 || enemy.air_force == 0;
    case GameMode::LAND_BATTLE:
        return enemy.commander == 0 ||
               (enemy.tank == 0 && enemy.infantry == 0 && enemy.artillery == 0);
    case GameMode::FULL_BATTLE:
    default:
        return enemy.commander == 0;
    }
}

// === NEW: Low-Depth Fortress & Special Draw Recognizer (~+30 Elo) ===
// Triggered only at depth <= 3 (AB) and q_depth <= 3 (QSearch).
// Handles:
//  • objective-complete decisive states (variant-specific)
//  • practical fortress/no-progress draws
//  • carrier-stacking loop-like dead-draw signatures
static bool low_depth_special_outcome(SearchState& st, const std::string& perspective,
                                      int depth_hint, int* out_score) {
    if (!out_score || depth_hint > 3) return false;

    const std::string enemy = opp(perspective);
    ObjectiveCounts me = collect_objective_counts(st.pieces, perspective);
    ObjectiveCounts them = collect_objective_counts(st.pieces, enemy);

    // Objective-based decisive recognizer (independent of "last mover").
    bool me_wins = side_fulfills_win_objective(me, them);
    bool them_wins = side_fulfills_win_objective(them, me);
    if (me_wins || them_wins) {
        if (me_wins && them_wins) {
            *out_score = 0;
            return true;
        }
        int base = 36000 + std::max(0, std::min(depth_hint, 6)) * 80;
        *out_score = me_wins ? base : -base;
        return true;
    }

    if (depth_hint <= 0) return false;
    if (me.commander == 0 || them.commander == 0) return false;

    ensure_attack_cache(st);
    int my_pi = player_idx(perspective);
    int op_pi = 1 - my_pi;
    int my_cc = st.cmd_col[my_pi], my_cr = st.cmd_row[my_pi];
    int op_cc = st.cmd_col[op_pi], op_cr = st.cmd_row[op_pi];
    if (my_cc < 0 || op_cc < 0) return false;

    // Fortress recognizer requires both commanders to be currently safe.
    if (st.atk.counts[op_pi][my_cr][my_cc] > 0) return false;
    if (st.atk.counts[my_pi][op_cr][op_cc] > 0) return false;

    const int total_active = me.active_non_hq + them.active_non_hq;
    if (total_active > 12) return false;

    AllMoves my_moves = all_moves_for(st.pieces, perspective);
    AllMoves op_moves = all_moves_for(st.pieces, enemy);
    if (my_moves.empty() || op_moves.empty()) return false;

    auto classify_activity = [&](const std::string& side,
                                 const AllMoves& moves,
                                 const Piece* enemy_cmd,
                                 int* captures,
                                 int* progress) {
        *captures = 0;
        *progress = 0;
        int inspected = 0;
        for (const auto& m : moves) {
            if (++inspected > 96) break; // cap for browser/runtime safety
            const Piece* tgt = piece_at_c(st.pieces, m.dc, m.dr);
            if (tgt && tgt->player != side) { (*captures)++; continue; }

            int idx = find_piece_idx_by_id(st.pieces, m.pid);
            if (idx < 0 || !enemy_cmd) continue;
            const Piece& p = st.pieces[idx];
            if (p.kind == "C" || p.kind == "H") continue;
            int before = std::abs(p.col - enemy_cmd->col) + std::abs(p.row - enemy_cmd->row);
            int after = std::abs(m.dc - enemy_cmd->col) + std::abs(m.dr - enemy_cmd->row);
            if (after + 1 < before) (*progress)++;
        }
    };

    const Piece* my_enemy_cmd = nullptr;
    const Piece* op_enemy_cmd = nullptr;
    for (const auto& p : st.pieces) {
        if (p.player == enemy && p.kind == "C") my_enemy_cmd = &p;
        if (p.player == perspective && p.kind == "C") op_enemy_cmd = &p;
    }

    int my_caps = 0, my_progress = 0;
    int op_caps = 0, op_progress = 0;
    classify_activity(perspective, my_moves, my_enemy_cmd, &my_caps, &my_progress);
    classify_activity(enemy, op_moves, op_enemy_cmd, &op_caps, &op_progress);

    bool no_captures = (my_caps == 0 && op_caps == 0);
    bool low_mobility = ((int)my_moves.size() <= 18 && (int)op_moves.size() <= 18);
    bool no_progress = (my_progress <= 1 && op_progress <= 1);
    bool carrier_loop_signature = (me.carried_units + them.carried_units >= 4);

    if (no_captures && low_mobility && (no_progress || carrier_loop_signature)) {
        if (!has_immediate_winning_move(st.pieces, perspective) &&
            !has_immediate_winning_move(st.pieces, enemy)) {
            *out_score = 0;
            return true;
        }
    }

    return false;
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

static bool is_win_condition_piece_kind(const std::string& kind) {
    return kind == "N" || kind == "Af" || kind == "T" || kind == "In" || kind == "A";
}

// === NEW: Advanced Threat Evaluation (~+80 Elo) ===
// Fast classical threat model:
//  • hanging/undefended pressure (scaled by unit value + carrier payload)
//  • cross-domain threats (Af/Navy/Artillery/Missile pressure)
//  • commander pressure and win-condition target pressure
//  • potential discovered attacks from loaded carriers (unload threats)
static int side_advanced_threat_score(const PieceList& pieces,
                                      const std::string& side,
                                      const AttackCache* cache,
                                      const MoveGenContext& ctx) {
    const std::string enemy = opp(side);
    const int side_pi = player_idx(side);
    const int enemy_pi = 1 - side_pi;
    int score = 0;

    const Piece* enemy_cmd = nullptr;
    for (const auto& p : pieces) {
        if (p.player == enemy && p.kind == "C") { enemy_cmd = &p; break; }
    }

    std::array<int, PieceList::kMaxPieces> payload_count{};
    for (const auto& p : pieces) {
        if (p.carrier_id >= 0 && p.carrier_id < (int)payload_count.size()) payload_count[p.carrier_id]++;
    }

    if (enemy_cmd) {
        int direct = cache ? cache->counts[side_pi][enemy_cmd->row][enemy_cmd->col]
                           : attackers_to_square(pieces, enemy_cmd->col, enemy_cmd->row, side, cache);
        int defenders = cache ? cache->counts[enemy_pi][enemy_cmd->row][enemy_cmd->col]
                              : attackers_to_square(pieces, enemy_cmd->col, enemy_cmd->row, enemy, cache);
        score += direct * 120;
        score += std::max(0, direct - defenders) * 170;
    }

    // Undefended / overloaded enemy pieces (higher value for carriers + objective units).
    for (const auto& ep : pieces) {
        if (ep.player != enemy || ep.kind == "H") continue;
        int atk = cache ? cache->counts[side_pi][ep.row][ep.col]
                        : attackers_to_square(pieces, ep.col, ep.row, side, cache);
        if (atk == 0) continue;
        int def = cache ? cache->counts[enemy_pi][ep.row][ep.col]
                        : attackers_to_square(pieces, ep.col, ep.row, enemy, cache);
        int val = piece_value_fast(ep.kind);
        int weight = val / 9;
        if (ep.kind == "C") weight += 260;
        if (ep.kind == "N" || ep.kind == "Af") weight += 140;
        if (is_win_condition_piece_kind(ep.kind)) weight += 80;
        if (ep.id >= 0 && ep.id < (int)payload_count.size() && payload_count[ep.id] > 0) {
            weight += 60 * payload_count[ep.id];
        }
        if (def == 0) score += weight + val / 4;
        else if (atk > def) score += weight / 2 + (atk - def) * 24;
        else if (atk == def && val >= 200) score += weight / 4;
    }

    // Cross-domain attack pressure + potential discovered attacks after unloading carriers.
    for (const auto& p : pieces) {
        if (p.player != side || p.kind == "H") continue;

        int payload = (p.id >= 0 && p.id < (int)payload_count.size()) ? payload_count[p.id] : 0;
        if (payload > 0 && enemy_cmd) {
            int cmd_dist = std::abs(p.col - enemy_cmd->col) + std::abs(p.row - enemy_cmd->row);
            if (cmd_dist <= 6) score += payload * std::max(0, 90 - cmd_dist * 12);
        }

        auto mvs = get_moves_with_ctx(p, ctx);
        for (const auto& mv : mvs) {
            const Piece* tgt = piece_at_c(pieces, mv.first, mv.second);
            if (!tgt || tgt->player == side) continue;

            int bonus = 0;
            if (p.kind == "Af") {
                if (tgt->kind != "Af") bonus += 36;
                if (is_sea(tgt->col, tgt->row) || tgt->kind == "N") bonus += 30;
            } else if (p.kind == "N") {
                if (is_sea(tgt->col, tgt->row) || tgt->kind == "N" || tgt->kind == "Af") bonus += 34;
            } else if (p.kind == "A" || p.kind == "Ms") {
                int dist = std::max(std::abs(p.col - tgt->col), std::abs(p.row - tgt->row));
                if (dist >= 2) bonus += 30 + dist * 4;
            }

            if (tgt->kind == "C") bonus += 160;
            if (is_win_condition_piece_kind(tgt->kind)) bonus += 48;
            score += bonus;
        }
    }

    // Loaded passengers near enemy commander indicate likely discovered-attack motifs.
    if (enemy_cmd) {
        for (const auto& p : pieces) {
            if (p.player != side || p.carrier_id < 0) continue;
            const Piece* carrier = piece_by_id_c(pieces, p.carrier_id);
            if (!carrier || carrier->player != side) continue;
            int dist = std::abs(carrier->col - enemy_cmd->col) + std::abs(carrier->row - enemy_cmd->row);
            if (dist > 7) continue;
            int payload_threat = piece_value_fast(p.kind) / 10;
            if (p.kind == "T" || p.kind == "A" || p.kind == "Ms" || p.kind == "Af" || p.kind == "C")
                payload_threat += 45;
            score += std::max(0, payload_threat + 70 - dist * 10);
        }
    }

    return score;
}

static int advanced_threat_eval(const PieceList& pieces, const std::string& perspective,
                                const AttackCache* cache = nullptr) {
    MoveGenContext ctx = build_movegen_context(pieces);
    int my_threats = side_advanced_threat_score(pieces, perspective, cache, ctx);
    int opp_threats = side_advanced_threat_score(pieces, opp(perspective), cache, ctx);
    return my_threats - opp_threats;
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
                auto mvs = get_moves(p, pieces);
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

    // === NEW: Advanced Threat Evaluation (~+80 Elo) ===
    // Uses attack-cache counts + single movegen context to keep the model fast.
    score += advanced_threat_eval(pieces, perspective, cache);

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
        auto cmd_moves = get_moves(*my_cmd, pieces);
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
        ensure_attack_cache(st);
        int precise = board_score(st.pieces, perspective, &st.atk, &perspective);
        stand = (stand * 2 + precise) / 3;
    }

    if (q_depth <= 3) {
        int special_score = 0;
        if (low_depth_special_outcome(st, perspective, 3 - q_depth, &special_score))
            return special_score;
    }

    // ── Commander in check detection (SF18 style) ─────────────────────────
    // When the side-to-move's commander is currently attacked, we cannot
    // apply the stand-pat cut-off or delta pruning — every move must be
    // tried (including quiet evasions) to avoid the horizon effect.
    bool in_check = false;
    {
        const Piece* my_cmd = nullptr;
        for (const auto& p : st.pieces) {
            if (p.player == perspective && p.kind == "C") { my_cmd = &p; break; }
        }
        if (my_cmd) {
            ensure_attack_cache(st);
            int pl_atk = (perspective == "red") ? 1 : 0;
            in_check = (st.atk.counts[pl_atk][my_cmd->row][my_cmd->col] > 0);
        }
    }

    if (!in_check) {
        if (stand >= beta) return beta;
        // Delta pruning: skip positions where even the best capture can't improve alpha
        if (stand < alpha - DELTA_MARGIN - 800) return alpha;
        if (alpha < stand) alpha = stand;
    }
    if (q_depth >= Q_LIMIT) return in_check ? stand : alpha;

    // Order captures by SEE score before iterating (stack-allocated, no heap)
    struct CapMove { int pid, dc, dr, see_val; bool is_quiet; };
    CapMove caps[128];  // enough for captures + commander evasions
    int ncaps = 0;
    for (auto& p : st.pieces) {
        if (p.player != perspective) continue;
        auto mvs = get_moves(p, st.pieces);
        for (auto& m : mvs) {
            const Piece* t = piece_at_c(st.pieces, m.first, m.second);
            bool is_cap = (t && t->player != perspective);
            // In check: also include quiet commander moves as evasions
            bool is_evasion = (in_check && p.kind == "C" && !is_cap);
            if (!is_cap && !is_evasion) continue;
            int sv = is_cap ? see(st.pieces, m.first, m.second, perspective) : 0;
            if (ncaps < 128) caps[ncaps++] = {p.id, m.first, m.second, sv, !is_cap};
        }
    }
    // Insertion sort (fast for small N, no allocations): captures first by SEE, then evasions
    for (int i = 1; i < ncaps; i++) {
        CapMove key = caps[i];
        int j = i - 1;
        // Sort: non-quiet (captures) ranked by SEE descending; quiet evasions last
        auto rank = [](const CapMove& c) { return c.is_quiet ? -100000 : c.see_val; };
        while (j >= 0 && rank(caps[j]) < rank(key)) {
            caps[j+1] = caps[j];
            j--;
        }
        caps[j+1] = key;
    }

    for (int ci = 0; ci < ncaps; ci++) {
        auto& c = caps[ci];
        // For quiet evasions (in-check only): no SEE or delta pruning
        if (!c.is_quiet) {
            // SEE pruning: skip clearly losing captures in quiescence
            if (c.see_val < 0 && q_depth >= 1) continue;
            // Delta pruning: skip captures that can't raise alpha even optimistically
            if (!in_check && c.see_val + stand + DELTA_MARGIN < alpha) continue;
        }
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
    // Stockfish 18 LMR formula: reduction ≈ ln(depth) * ln(moves) / 2.0
    // (slightly more aggressive than the old /2.25; improves search speed
    //  at depth ≥ 6 where later moves are almost always sub-optimal)
    for (int d = 0; d < 64; d++) {
        for (int m = 0; m < 64; m++) {
            if (d == 0 || m == 0) { g_lmr_table[d][m] = 0; continue; }
            g_lmr_table[d][m] = (int)(0.50 + std::log(d) * std::log(m) / 2.0);
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
    // WASM-SAFE: throttle wall-clock reads to once per 4096 node checks.
    if (((++g_time_check_counter) & 4095ULL) != 0) return false;
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

// Seed search repetition path from game history.
// The current root hash is expected to be added by SearchPathGuard, so when
// the history already ends with root_hash we drop one copy to avoid double-counting.
static void seed_search_hash_path_from_history(const std::vector<uint64_t>& history,
                                               uint64_t root_hash) {
    g_search_hash_path = history;
    if (!g_search_hash_path.empty() && g_search_hash_path.back() == root_hash) {
        g_search_hash_path.pop_back();
    }
}

static int alphabeta(SearchState& st, int depth, int alpha, int beta,
                     const std::string& cpu_player, int ply,
                     bool null_ok=true, const MoveTriple* prev_move=nullptr,
                     ThreadData* td=nullptr) {
    SearchPathGuard path_guard(st.hash);
    if (path_is_threefold(st.hash)) return 0;
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
    if (depth <= 3 && depth > 0) {
        int special_score = 0;
        if (low_depth_special_outcome(st, cpu_player, depth, &special_score))
            return special_score;
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
    // FIX: Do NOT mutate `depth` in-place — that corrupts RFP/razoring margins,
    // correction history update depth, and TT store depth. Use a separate variable.
    int search_depth = depth;
    if (!hash_move_ptr && depth >= 6 && !pv_node) {
        search_depth = depth - 1;
    }

    // ── Corrected static eval (Stockfish 18 Correction History) ────────────
    // Apply position- and material-indexed correction offsets to the cheap
    // quick_eval so that all depth-pruning thresholds (RFP, Razoring, Futility,
    // Probcut, LMR-improving) are based on a more accurate baseline.
    int raw_static_eval = st.quick_eval;
    int static_eval = corrected_static_eval(h, st.pieces, st.turn, raw_static_eval);

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

    // ── Reverse Futility Pruning (Stockfish 18 tuning, extended to depth 4) ─
    // SF18 pushes RFP to depth 4 with tighter margins; the correction-history
    // improved static eval makes these thresholds more reliable.
    if (pruning_safe && !pv_node && depth <= 4) {
        int rfp_margin = (improving ? 100 : 160) * depth + 80;
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
                                   cpu_player, ply, false, prev_move, td);
            if (pc_val >= probcut_beta) return pc_val;
        }
        if (!node_is_max && static_eval <= alpha - 200) {
            int probcut_alpha = alpha - 200;
            int pc_val = alphabeta(st, probcut_depth, probcut_alpha, probcut_alpha + 1,
                                   cpu_player, ply, false, prev_move, td);
            if (pc_val <= probcut_alpha) return pc_val;
        }
    }

    // ── Dynamic Null Move Pruning tuned for 11x12 volatility ──────────────
    bool stm_in_check = (commander_attackers_cached(st, st.turn) > 0);
    if (null_ok && depth >= 3 && !pv_node && !stm_in_check) {
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
                    // Minimax search: keep cpu_player perspective and do not negate.
                    null_val = alphabeta(ns, depth-1-R, beta-1, beta, cpu_player, ply+1, false, prev_move, td);
                } else {
                    null_val = alphabeta(ns, depth-1-R, alpha, alpha+1, cpu_player, ply+1, false, prev_move, td);
                }
                ns.turn = nu.turn_before;
                ns.hash = nu.hash_before;
                ns.quick_eval = nu.quick_eval_before;
                ns.atk.valid = false;
                ns.rebuild_caches();  // FIX: restore cached cmd positions/navy counts

                if (node_is_max) {
                    if (null_val >= beta) {
                        if (depth >= 8) {
                            int verify = alphabeta(st, depth-R-1, beta-1, beta, cpu_player, ply+1, false, prev_move, td);
                            if (verify >= beta) return beta;
                        } else {
                            return beta;
                        }
                    }
                } else {
                    if (null_val <= alpha) {
                        if (depth >= 8) {
                            int verify = alphabeta(st, depth-R-1, alpha, alpha+1, cpu_player, ply+1, false, prev_move, td);
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
        ensure_attack_cache(st);
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
    std::array<QuietEntry, 64> searched_quiets{};
    int searched_quiet_count = 0;

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
        int full_depth = search_depth - 1 + ((is_critical_capture && search_depth <= 4) ? 1 : 0);
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
            if (is_hash_move && tte && tte->depth >= search_depth - 1 && search_depth >= 5 &&
                !time_up() && std::abs(tt_val) < 30000) {
                int sing_beta = tt_val - 90;
                bool is_singular = true;
                int tested = 0, near_miss = 0;
                for (auto& om : moves) {
                    if (same_move(om, m)) continue;
                    if (tested >= 16 || time_up()) break;
                    UndoMove su;
                    if (!make_move_inplace(st, om, cpu_player, su)) continue;
                    int sv = alphabeta(st, search_depth - 2, sing_beta - 1, sing_beta, cpu_player, ply + 1, false, &om, td);
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
            } else if (!is_hash_move && tte && search_depth >= 5 &&
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
        if (ext_depth >= search_depth) ext_depth = search_depth - 1;
        if (ext_depth < 0) ext_depth = 0;
        int child;
        if (move_index == 0) {
            // PV move: full window
            child = alphabeta(st, ext_depth, alpha, beta, cpu_player, ply+1, true, &m, td);
        } else {
            int new_depth = ext_depth;

            // ── LMR: table-driven reductions for late quiet moves ────────
            if (is_quiet && move_index >= 2 && search_depth >= 2) {
                int R = lmr_reduction(search_depth, move_index);
                if (pv_node) R -= 1;
                if (improving) R -= 1;
                if (!improving && search_depth >= 6) R += 1;
                // SF18: history-based LMR adjustment
                // Good history → reduce less; bad history → reduce more.
                if (moved_ki >= 0) {
                    int hval = td ? td_history_score(*td, hist_pl, moved_ki, m.dc, m.dr)
                                  : history_score(hist_pl, moved_ki, m.dc, m.dr);
                    R -= hval / 6000;  // ±5 range from history
                }
                if (R < 0) R = 0;
                new_depth = ext_depth - R;
                if (new_depth < 1) new_depth = 1;
            }

            // Zero-window (PVS) search
            if (node_is_max)
                child = alphabeta(st, new_depth, alpha, alpha+1, cpu_player, ply+1, true, &m, td);
            else
                child = alphabeta(st, new_depth, beta-1,  beta, cpu_player, ply+1, true, &m, td);

            // Re-search at full depth if LMR-reduced search beats the bound
            bool lmr_fail = node_is_max ? (child > alpha) : (child < beta);
            if (new_depth < ext_depth && lmr_fail) {
                if (pv_node) {
                    // PV node: re-search directly with full window (skip extra ZW)
                    child = alphabeta(st, ext_depth, alpha, beta, cpu_player, ply+1, true, &m, td);
                } else {
                    if (node_is_max)
                        child = alphabeta(st, ext_depth, alpha, alpha+1, cpu_player, ply+1, true, &m, td);
                    else
                        child = alphabeta(st, ext_depth, beta-1, beta, cpu_player, ply+1, true, &m, td);
                }
            }

            // Full-window re-search if PVS fails in PV node (only needed when LMR wasn't already re-searched full)
            if (!lmr_fail || new_depth >= ext_depth) {
                bool pvs_fail = node_is_max ? (child > alpha && child < beta)
                                            : (child < beta  && child > alpha);
                if (pvs_fail && pv_node) {
                    child = alphabeta(st, ext_depth, alpha, beta, cpu_player, ply+1, true, &m, td);
                }
            }
        }

        unmake_move_inplace(st, u);

        // Track searched quiet moves for history malus
        if (is_quiet && moved_ki >= 0) {
            if (searched_quiet_count < (int)searched_quiets.size()) {
                searched_quiets[searched_quiet_count++] = {moved_ki, m.dc, m.dr};
            }
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
                    for (int qi = 0; qi < searched_quiet_count; qi++) {
                        const auto& sq = searched_quiets[qi];
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
                    for (int qi = 0; qi < searched_quiet_count; qi++) {
                        const auto& sq = searched_quiets[qi];
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
        ensure_attack_cache(st);
        return board_score(st.pieces, cpu_player, &st.atk, &st.turn);
    }

    int flag = (val<=orig_alpha) ? TT_UPPER : (val>=orig_beta ? TT_LOWER : TT_EXACT);
    tt_store(h, depth, flag, val, best_move);
    // ── Update Correction History (Stockfish 18) ──────────────────────────
    // Only update on EXACT (inside-window) nodes: fail-high/low scores are
    // one-sided bounds and would bias the correction in the wrong direction.
    // FIX: Both search_val and raw_static_eval must be from the same
    // player's perspective. raw_static_eval is always from cpu_player's view.
    // For min nodes (opponent's turn), negate both so the correction is
    // computed from the opponent's perspective before storing.
    if (flag == TT_EXACT && depth >= 1 && std::abs(val) < 20000) {
        int corr_val = node_is_max ? val : -val;
        int corr_static = node_is_max ? raw_static_eval : -raw_static_eval;
        update_correction_history(h, st.pieces, node_is_max ? cpu_player : opp(cpu_player),
                                  depth, corr_val, corr_static);
    }
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
//   • Leaf:  Alpha-beta at engine_mcts_ab_depth() plies — acts as the "value head".
//   • This eliminates the horizon effect that pure AB suffers at the root:
//     promising moves receive exponentially more AB evaluations, naturally
//     extending the effective search horizon.
// ────────────────────────────────────────────────────────────────────────────

static constexpr float MCTS_CPUCT    = 1.8f;  // exploration constant
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
    const std::vector<uint64_t> game_rep_history_copy = g_game_rep_history;
    seed_search_hash_path_from_history(g_game_rep_history, root_st.hash);
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
    EngineMutex tree_mutex;
    const std::string opp_player = opp(cpu_player);

    auto evaluate_leaf_value = [&](SelectionPath& sel, int* out_val) -> bool {
        if (!out_val) return false;
        int val = alphabeta(sel.eval_st, ab_depth, -999999, 999999,
                            cpu_player, (sel.l2_idx >= 0 ? 2 : 1), true, &sel.prev_move, nullptr);
        if (time_up()) return false;
        *out_val = val;
        return true;
    };

    auto apply_leaf_result = [&](const SelectionPath& sel, float leaf_val) -> bool {
        std::lock_guard<EngineMutex> lk(tree_mutex);
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
        std::lock_guard<EngineMutex> lk(tree_mutex);
        if (sel.l1_idx < 0 || sel.l1_idx >= (int)children.size()) return;
        MCTSLevel1Child& l1 = children[sel.l1_idx];
        if (l1.virtual_loss > 0) l1.virtual_loss--;
        if (sel.l2_idx >= 0 && sel.l2_idx < (int)l1.children.size()) {
            MCTSLevel2Child& l2 = l1.children[sel.l2_idx];
            if (l2.virtual_loss > 0) l2.virtual_loss--;
        }
    };

    auto select_path = [&](SelectionPath& sel) -> bool {
        std::lock_guard<EngineMutex> lk(tree_mutex);
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
        g_game_rep_history = game_rep_history_copy;
        seed_search_hash_path_from_history(g_game_rep_history, root_st.hash);
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
                ensure_attack_cache(selected[i].eval_st);
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

    int num_workers = 1;
#if COMMANDER_ENABLE_THREADS
    if (!get_engine_config().force_single_thread) {
        int hw_threads = (int)std::thread::hardware_concurrency();
        if (hw_threads <= 0) hw_threads = 1;
        num_workers = std::max(1, std::min(hw_threads, MCTS_MAX_THREADS));
    }
#endif
    if (time_limit_secs <= 0.10 || (int)children.size() <= 2) num_workers = 1;

    if (num_workers == 1) {
        worker(0);
    } else {
#if COMMANDER_ENABLE_THREADS
        std::vector<std::thread> threads;
        threads.reserve(num_workers);
        for (int i = 0; i < num_workers; i++) threads.emplace_back(worker, i);
        for (auto& t : threads) if (t.joinable()) t.join();
#else
        worker(0);
#endif
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
    if (st.pieces.size() < 34) return false; // opening only
    const bool very_early_opening = (st.pieces.size() >= 36);
    auto prow = [&](int blue_row) { return (cpu_player == "red") ? (ROWS - 1 - blue_row) : blue_row; };

    std::vector<MoveTriple> book;

    // Prefer early navy stabilization + air control in opening (perspective-aware).
    const int r_back = prow(10);
    const int r_front = prow(8);
    const Piece* navy_back = piece_at_c(st.pieces, 0, r_back);
    const Piece* navy_front = piece_at_c(st.pieces, 0, r_front);
    bool have_back_navy = navy_back && navy_back->player == cpu_player && navy_back->kind == "N";
    bool have_front_navy = navy_front && navy_front->player == cpu_player && navy_front->kind == "N";
    if (have_back_navy && have_front_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, r_front, 1, r_front);
        append_book_move_from_square(book, st, cpu_player, 0, r_back, 1, r_back);
    } else if (have_back_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, r_back, 1, r_back);
    } else if (have_front_navy) {
        append_book_move_from_square(book, st, cpu_player, 0, r_front, 1, r_front);
    }
    // Delay non-capture AF flights in the very first turns.
    if (!very_early_opening) {
        append_book_move_from_square(book, st, cpu_player, 3, prow(7), 2, prow(7));
        append_book_move_from_square(book, st, cpu_player, 7, prow(7), 8, prow(7));
        append_book_move_from_square(book, st, cpu_player, 3, prow(7), 3, prow(8));
        append_book_move_from_square(book, st, cpu_player, 7, prow(7), 7, prow(8));
    }
    append_book_move_from_square(book, st, cpu_player, 5, prow(7), 5, prow(6));
    append_book_move_from_square(book, st, cpu_player, 4, prow(8), 4, prow(7));
    append_book_move_from_square(book, st, cpu_player, 6, prow(8), 6, prow(7));

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

static AIResult cpu_pick_move(const PieceList& pieces, const std::string& cpu_player,
                               int max_depth, double time_limit_secs,
                               const std::atomic<bool>* stop_flag = nullptr,
                               ThreadData* td = nullptr) {
    const EngineConfig& cfg = get_engine_config();
    if (max_depth <= 0) max_depth = std::max(1, cfg.max_depth);
    if (time_limit_secs <= 0.0) {
        time_limit_secs = std::max(0.01, cfg.time_limit_ms / 1000.0);
    }

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
    g_nodes.store(0, std::memory_order_relaxed);

    SearchState root = make_search_state(pieces, cpu_player, cpu_player);
    // Seed search path with game-level repetition history so the engine
    // avoids moves that create threefold repetition with prior positions.
    seed_search_hash_path_from_history(g_game_rep_history, root.hash);
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
                    val = alphabeta(root, cur_depth-1, window_alpha, window_beta,
                                    cpu_player, 1, true, &m, td);
                } else {
                    // PVS: zero-window search first
                    val = alphabeta(root, cur_depth-1, window_alpha, window_alpha+1,
                                    cpu_player, 1, true, &m, td);
                    // Re-search with full window if it beats alpha but doesn't reach beta
                    if (val > window_alpha && val < window_beta) {
                        val = alphabeta(root, cur_depth-1, window_alpha, window_beta,
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
#if !COMMANDER_ENABLE_THREADS
    return 1;
#else
    if (get_engine_config().force_single_thread) return 1;
    if (g_smp_thread_count > 0) return g_smp_thread_count;
    int hw = (int)std::thread::hardware_concurrency();
    if (hw <= 0) hw = 1;
    // Use all available hardware threads by default.
    return std::max(hw, 1);
#endif
}

struct SMPShared {
    std::atomic<bool>   stop{false};
    std::atomic<int>    best_score{-999999};
    EngineMutex         best_mutex;
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
    // Each thread gets its own ThreadData
    ThreadData td;
    td.thread_id = thread_id;
    td.reset();

    // Set up stop flag and deadline for this thread
    g_deadline = shared.deadline;

    SearchState root = make_search_state(pieces, cpu_player, cpu_player);
    seed_search_hash_path_from_history(g_game_rep_history, root.hash);
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

        // ── Aspiration Windows (Stockfish 18 tuning) ─────────────────────
        // δ=10 at depth≥6, δ=25 at depth 4-5, full window earlier.
        // Tighter than the old δ=40/60 values for less re-search waste.
        int delta;
        if (cur_depth >= 6)      delta = 10;
        else if (cur_depth >= 4) delta = 25;
        else                     delta = 60;
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
                    val = alphabeta(root, cur_depth-1, window_alpha, window_beta,
                                    cpu_player, 1, true, &m, &td);
                } else {
                    // PVS: zero-window search first
                    val = alphabeta(root, cur_depth-1, window_alpha, window_alpha+1,
                                    cpu_player, 1, true, &m, &td);
                    if (val > window_alpha && val < window_beta) {
                        val = alphabeta(root, cur_depth-1, window_alpha, window_beta,
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

            // ── Asymmetric aspiration widening (Stockfish 18 style) ────────
            // Only expand in the direction of failure; 1.44× growth + 5
            // converges faster than the old ×2 symmetric expansion and
            // wastes fewer re-searches when score drifts in one direction.
            delta = (int)(delta * 1.44f) + 5;
            if (cur_best_val <= alpha) alpha = std::max(-999999, cur_best_val - delta);
            else                       beta  = std::min( 999999, cur_best_val + delta);
            if (delta > 800) break;
        }

        if (completed) best = cur_best;
        if (completed) prev_score = cur_best_val;

        // Report to shared best if this thread found something good
        if (completed) {
            int global_best = shared.best_score.load(std::memory_order_relaxed);
            if (cur_best_val > global_best || !shared.best_found) {
                std::lock_guard<EngineMutex> lk(shared.best_mutex);
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
                bool move_changed = !(best.pid == prev_pid && best.dc == prev_dc && best.dr == prev_dr);
                if (!move_changed) {
                    shared.best_move_stability.fetch_add(1, std::memory_order_relaxed);
                } else {
                    shared.best_move_stability.store(0, std::memory_order_relaxed);
                    // ── SF18-style node-count time extension ─────────────────
                    // When the best move changes, the position is tactically
                    // complex: extend the soft deadline by 25% (capped at hard).
                    // This mirrors Stockfish 18's "bestMoveChanges" time manager.
                    if (cur_depth >= 4) {
                        std::lock_guard<EngineMutex> lk(shared.best_mutex);
                        auto now = std::chrono::steady_clock::now();
                        auto remaining = shared.deadline - now;
                        auto extension = remaining / 4;  // 25% of remaining time
                        auto new_soft = shared.soft_deadline + extension;
                        // Never push soft past hard limit
                        if (new_soft < shared.deadline)
                            shared.soft_deadline = new_soft;
                    }
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
    if (get_engine_config().force_single_thread) num_threads = 1; // WASM-SAFE

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

    const std::vector<uint64_t> game_rep_history_copy = g_game_rep_history;
    auto run_worker = [&](int thread_id) {
        g_stop_flag = external_stop;
        g_deadline = shared.deadline;
        g_game_rep_history = game_rep_history_copy;
        reset_time_state();
        smp_worker(thread_id, pieces, cpu_player, max_depth, shared);
    };

    if (num_threads <= 1) {
        run_worker(0);
    } else {
#if COMMANDER_ENABLE_THREADS
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&, i]() { run_worker(i); });
        }
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
#else
        run_worker(0);
#endif
    }

    if (shared.best_found)
        return {true, shared.best_move};

    return {false, {}};
}
// COORDINATE HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static int cx(int col) { return PAD + col*CELL; }
static int cy(int row) { return PAD + (ROWS-1-row)*CELL; }

static void canvas_to_board(int px, int py, int& col, int& row) {
    col = (int)std::round((float)(px - PAD) / CELL);
    row = ROWS - 1 - (int)std::round((float)(py - PAD) / CELL);
}

// ═══════════════════════════════════════════════════════════════════════════
// TEXTURE LOADING FROM EMBEDDED BASE64
// ═══════════════════════════════════════════════════════════════════════════

static std::map<std::string, SDL_Texture*> g_textures;
static std::map<std::string, SDL_Texture*> g_textures_small;

static SDL_Texture* load_texture_from_b64(SDL_Renderer* ren, const std::string& key, int target_size) {
    auto it = PIECE_B64.find(key);
    if (it == PIECE_B64.end()) return nullptr;

    std::vector<uint8_t> png_data = base64_decode(it->second);
    SDL_RWops* rw = SDL_RWFromMem(png_data.data(), (int)png_data.size());
    if (!rw) return nullptr;

    SDL_Surface* surf = IMG_LoadPNG_RW(rw);
    SDL_RWclose(rw);
    if (!surf) return nullptr;

    // Scale to target_size x target_size
    SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, target_size, target_size, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_BlitScaled(surf, nullptr, scaled, nullptr);
    SDL_FreeSurface(surf);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, scaled);
    SDL_FreeSurface(scaled);
    return tex;
}

static void load_all_textures(SDL_Renderer* ren) {
    ensure_piece_b64_loaded();
    for (auto& kv : PIECE_B64) {
        SDL_Texture* t = load_texture_from_b64(ren, kv.first, 50);
        if (t) g_textures[kv.first] = t;
        SDL_Texture* ts = load_texture_from_b64(ren, kv.first, 20);
        if (ts) g_textures_small[kv.first] = ts;
    }
}
// GAME STATE
// ═══════════════════════════════════════════════════════════════════════════

enum class GameState { HUMAN_TURN, CPU_THINKING, GAME_OVER };

struct MoveRecord {
    int from_c = 0;
    int from_r = 0;
    int to_c = 0;
    int to_r = 0;
    bool capture = false;
    std::string player = "red";
};

struct Game {
    PieceList pieces;
    std::string current;
    int selected_id = -1;
    std::vector<Move2> valid_moves;
    GameState state = GameState::HUMAN_TURN;
    std::string win_msg;
    std::vector<std::string> move_log;
    std::vector<MoveRecord> move_records;
    std::vector<PieceList> state_history;      // board snapshot at ply index (0 = initial)
    std::vector<std::string> turn_history;     // side-to-move for each snapshot
    std::vector<uint64_t> position_history;
    int cpu_depth = 6;
    double cpu_time_limit = 3.0;
    std::string status_msg;
    int difficulty = 1; // 0=Easy, 1=Medium, 2=Hard
    GameMode selected_mode = GameMode::FULL_BATTLE;
    bool show_mode_menu = true;
    std::string human_player = "red";
    std::string cpu_player = "blue";
    std::string menu_side_choice = "red";
    bool menu_side_chosen = false;
    bool has_last_move = false;
    int last_from_c = 0;
    int last_from_r = 0;
    int last_to_c = 0;
    int last_to_r = 0;
    bool last_move_capture = false;
    std::string last_move_player = "red";
    int review_index = -1; // -1 = live board, else index into state_history

    // === CHANGED ===
    // CPU async (WASM-SAFE: synchronous fallback when threads are disabled).
#if COMMANDER_ENABLE_THREADS
    std::thread cpu_thread;
#endif
    EngineMutex cpu_mutex;
    std::atomic<bool> cpu_stop{false};
    bool cpu_done = false;
    AIResult cpu_result;

    Game() {
        pieces = make_initial_pieces();
        current = "red";
        state_history.clear();
        turn_history.clear();
        state_history.push_back(pieces);
        turn_history.push_back(current);
        position_history.clear();
        push_position_history(position_history, zobrist_hash(pieces, current));
        set_difficulty(difficulty);
        g_game_mode = selected_mode;
        status_msg = "Select mode, choose side, then click START GAME";
    }
    ~Game() { stop_cpu(); }

    void stop_cpu() {
        cpu_stop.store(true, std::memory_order_relaxed);
#if COMMANDER_ENABLE_THREADS
        if (cpu_thread.joinable()) cpu_thread.join();
#endif
        cpu_stop.store(false, std::memory_order_relaxed);
    }

    void set_difficulty(int d) {
        difficulty = d;
        EngineConfig cfg = get_engine_config();
        if (d==0) {
            cpu_depth=4; cpu_time_limit=2.5;
            cfg.use_mcts=false;
            cfg.max_depth=cpu_depth;
            cfg.time_limit_ms=(int)(cpu_time_limit * 1000.0);
        } else if (d==1) {
            cpu_depth=6; cpu_time_limit=3.0;
            cfg.use_mcts=false;
            cfg.max_depth=cpu_depth;
            cfg.time_limit_ms=(int)(cpu_time_limit * 1000.0);
        } else {
            cpu_depth=8; cpu_time_limit=8.0;
            cfg.use_mcts=true;
            cfg.max_depth=cpu_depth;
            cfg.time_limit_ms=(int)(cpu_time_limit * 1000.0);
        }
#if !COMMANDER_ENABLE_THREADS
        cfg.force_single_thread = true;
#endif
        set_engine_config(cfg);
    }

    void set_game_mode(GameMode mode) {
        selected_mode = mode;
        if (show_mode_menu) {
            if (menu_side_chosen) {
                status_msg = std::string("Mode selected: ") + game_mode_name(selected_mode) +
                             " | YOU = " + (menu_side_choice == "red" ? "RED" : "BLUE") +
                             " (click START GAME)";
            } else {
                status_msg = std::string("Mode selected: ") + game_mode_name(selected_mode) +
                             " (choose side, then START GAME)";
            }
        } else {
            status_msg = std::string("Mode selected: ") + game_mode_name(selected_mode) + " (applies on New Game)";
        }
    }

    void set_player_side(const std::string& side) {
        if (side != "red" && side != "blue") return;
        menu_side_choice = side;
        menu_side_chosen = true;
        if (show_mode_menu) {
            status_msg = std::string("Side selected: YOU = ") +
                         (side == "red" ? "RED" : "BLUE") +
                         " (click START GAME)";
        }
    }

    void open_mode_menu() {
        stop_cpu();
        show_mode_menu = true;
        review_index = -1;
        state = GameState::HUMAN_TURN;
        selected_id = -1;
        valid_moves.clear();
        win_msg.clear();
        menu_side_choice = human_player;
        menu_side_chosen = false;
        status_msg = std::string("Select mode: ") + game_mode_name(selected_mode) +
                     " — choose side, then START GAME";
    }

    void start_selected_mode_game() {
        if (!menu_side_chosen) {
            status_msg = "Choose your side first (RED or BLUE), then START GAME";
            return;
        }
        human_player = menu_side_choice;
        cpu_player = opp(human_player);
        show_mode_menu = false;
        new_game();
    }

    void set_status(const char* msg) {
        if (msg) { status_msg = msg; return; }
        if (current == human_player) {
            status_msg = std::string("Your turn (") +
                         (human_player == "red" ? "RED" : "BLUE") +
                         ") — select a piece to move";
        } else {
            status_msg = std::string("CPU thinking (") +
                         (cpu_player == "red" ? "RED" : "BLUE") +
                         ")...";
        }
    }

    Piece* get_piece(int id) {
        for (auto& p : pieces) if (p.id==id) return &p;
        return nullptr;
    }

    const PieceList& board_for_render() const {
        if (review_index >= 0 && review_index < (int)state_history.size())
            return state_history[review_index];
        return pieces;
    }

    bool is_reviewing() const {
        return review_index >= 0 && review_index < (int)state_history.size();
    }

    void set_review_status() {
        if (!is_reviewing()) return;
        if (review_index <= 0 || move_records.empty()) {
            status_msg = "Reviewing initial setup — use < > arrows or click board for LIVE";
            return;
        }
        int mv = review_index - 1;
        if (mv < 0 || mv >= (int)move_records.size()) {
            status_msg = "Reviewing history — use < > arrows or click board for LIVE";
            return;
        }
        const MoveRecord& mr = move_records[mv];
        status_msg = "Reviewing move " + std::to_string(mv + 1) + "/" +
                     std::to_string(move_records.size()) +
                     " (" + (mr.player == "red" ? "RED" : "BLUE") +
                     ") — use < > arrows or click board for LIVE";
    }

    void exit_review_mode() {
        if (!is_reviewing()) return;
        review_index = -1;
        set_status(nullptr);
    }

    void review_move_at(int move_idx) {
        if (move_idx < 0 || move_idx >= (int)move_records.size()) return;
        if (move_idx == (int)move_records.size() - 1) {
            // Clicking latest move snaps back to live board.
            exit_review_mode();
            return;
        }
        int target = move_idx + 1; // snapshot after this move
        if (target < 0 || target >= (int)state_history.size()) return;
        review_index = target;
        selected_id = -1;
        valid_moves.clear();
        set_review_status();
    }

    void review_prev_move() {
        if (state_history.size() <= 1) return;
        if (!is_reviewing()) {
            review_index = (int)state_history.size() - 2; // move before live
        } else if (review_index > 0) {
            review_index -= 1;
        }
        selected_id = -1;
        valid_moves.clear();
        set_review_status();
    }

    void review_next_move() {
        if (!is_reviewing()) return;
        int live_idx = (int)state_history.size() - 1;
        if (review_index >= live_idx - 1) {
            exit_review_mode(); // next from latest reviewed move -> live
            return;
        }
        review_index += 1;
        selected_id = -1;
        valid_moves.clear();
        set_review_status();
    }

    void new_game() {
        stop_cpu();
        g_game_mode = selected_mode;
        pieces = make_initial_pieces();
        current = "red"; // Red always starts; CPU may move first if user picked Blue.
        selected_id = -1;
        valid_moves.clear();
        state = GameState::HUMAN_TURN;
        win_msg.clear();
        move_log.clear();
        move_records.clear();
        has_last_move = false;
        review_index = -1;
        state_history.clear();
        turn_history.clear();
        state_history.push_back(pieces);
        turn_history.push_back(current);
        position_history.clear();
        push_position_history(position_history, zobrist_hash(pieces, current));
        cpu_done = false;
        reset_search_tables();
        if (current == cpu_player) {
            state = GameState::CPU_THINKING;
            status_msg = g_use_mcts ? "CPU thinking (MCTS)..." : "CPU is thinking...";
            play_sound("cpu");
            start_cpu_move();
        } else {
            set_status(nullptr);
        }
    }

    void on_click(int px, int py) {
        if (is_reviewing()) {
            exit_review_mode();
            return;
        }
        if (state != GameState::HUMAN_TURN) return;
        int col, row;
        canvas_to_board(px, py, col, row);
        if (!on_board(col,row)) { selected_id=-1; valid_moves.clear(); set_status(nullptr); return; }

        // If something selected and clicked valid move destination
        if (selected_id >= 0) {
            for (auto& m : valid_moves) {
                if (m.first==col && m.second==row) {
                    execute_move(col,row,false);
                    return;
                }
            }
        }

        // Select piece (cycle through stacked same-square pieces on repeated click).
        std::vector<Piece*> stack_candidates;
        for (auto& p : pieces) {
            if (p.player == current && p.col == col && p.row == row) stack_candidates.push_back(&p);
        }
        if (!stack_candidates.empty()) {
            Piece* clicked = stack_candidates[0];
            if (stack_candidates.size() > 1) {
                int cur_idx = -1;
                for (int i = 0; i < (int)stack_candidates.size(); i++) {
                    if (stack_candidates[i]->id == selected_id) { cur_idx = i; break; }
                }
                if (cur_idx >= 0) clicked = stack_candidates[(cur_idx + 1) % (int)stack_candidates.size()];
            }
            selected_id = clicked->id;
            valid_moves = get_moves(*clicked, pieces);
            if (valid_moves.empty()) play_sound("invalid");
            std::string nm = std::to_string(valid_moves.size());
            status_msg = "Selected " + PIECE_DEF[clicked->kind].name +
                         (clicked->hero ? " ★" : "") +
                         " — " + nm + " move" + (valid_moves.size()!=1?"s":"");
        } else {
            selected_id = -1; valid_moves.clear(); set_status(nullptr);
        }
    }

    void execute_move(int dc, int dr, bool is_cpu) {
        Piece* piece = get_piece(selected_id);
        if (!piece) return;
        if (!has_legal_destination(*piece, pieces, dc, dr)) {
            play_sound("invalid");
            return;
        }
        Piece before_piece = *piece;
        Piece* target = piece_at(pieces, dc, dr);

        std::string log = (is_cpu?"CPU ":"YOU ") + piece->kind +
                          "(" + std::to_string(piece->col) + "," + std::to_string(piece->row) + ")" +
                          "->(" + std::to_string(dc) + "," + std::to_string(dr) + ")";

        int enemy_before = 0;
        int own_before = 0;
        for (const auto& p : pieces) {
            if (p.player == current) own_before++;
            else enemy_before++;
        }

        if (target && target->player != current) log += " x" + target->kind;
        pieces = apply_move(pieces, selected_id, dc, dr, current);

        int enemy_after = 0;
        int own_after = 0;
        for (const auto& p : pieces) {
            if (p.player == current) own_after++;
            else enemy_after++;
        }
        bool is_capture = (enemy_after < enemy_before);
        bool self_lost_piece = (own_after < own_before);
        MoveRecord rec;
        rec.from_c = before_piece.col;
        rec.from_r = before_piece.row;
        rec.to_c = dc;
        rec.to_r = dr;
        rec.capture = is_capture;
        rec.player = current;
        has_last_move = true;
        last_from_c = before_piece.col;
        last_from_r = before_piece.row;
        last_to_c = dc;
        last_to_r = dr;
        last_move_capture = is_capture;
        last_move_player = current;

        Piece* after_piece = get_piece(selected_id);
        if (before_piece.kind == "Af" && is_capture && self_lost_piece && !after_piece) {
            log += " (kamikaze)";
            play_sound("boom");
        } else {
            play_sound(is_capture ? "capture" : "move");
        }

        if (after_piece && !before_piece.hero && after_piece->hero) {
            log += " *HERO";
            play_sound("hero");
        }
        if (after_piece && before_piece.kind == "Af" &&
            before_piece.col == after_piece->col && before_piece.row == after_piece->row &&
            is_capture && (dc != before_piece.col || dr != before_piece.row)) {
            log += " @RETURN";
        }

        finish_turn(log, rec);
    }

    void finish_turn(const std::string& log, const MoveRecord& rec) {
        if (is_reviewing()) review_index = -1;
        move_log.push_back(log);
        move_records.push_back(rec);

        std::string wm = check_win(pieces, current);
        if (!wm.empty()) {
            state_history.push_back(pieces);
            turn_history.push_back(opp(current));
            state = GameState::GAME_OVER;
            win_msg = wm;
            selected_id=-1; valid_moves.clear();
            play_sound("win");
            return;
        }

        current = opp(current);
        state_history.push_back(pieces);
        turn_history.push_back(current);
        uint64_t cur_hash = zobrist_hash(pieces, current);
        push_position_history(position_history, cur_hash);
        if (is_threefold_repetition(position_history, cur_hash)) {
            state = GameState::GAME_OVER;
            win_msg = "Draw — threefold repetition.";
            selected_id = -1;
            valid_moves.clear();
            return;
        }

        selected_id=-1; valid_moves.clear();
        set_status(nullptr);

        if (current == cpu_player) {
            state = GameState::CPU_THINKING;
            status_msg = g_use_mcts ? "CPU thinking (MCTS)..." : "CPU is thinking...";
            play_sound("cpu");
            start_cpu_move();
        }
    }

    void start_cpu_move() {
        stop_cpu();
        {
            std::lock_guard<EngineMutex> lk(cpu_mutex);
            cpu_done = false;
        }
        PieceList pieces_copy = pieces;
        std::string cpu_pl = cpu_player;
        int depth = cpu_depth;
        double tlimit = cpu_time_limit;

        auto run_cpu_search = [this, pieces_copy, cpu_pl, depth, tlimit]() {
            try {
                reset_search_tables();
                g_game_rep_history = position_history;  // let search see game repetition history
                AIResult res;
                if (g_use_mcts) {
                    // Hard difficulty: MCTS-guided root with AB value function
                    // Opening book is checked inside smp_cpu_pick_move, but we
                    // also check it here so MCTS skips it.
                    {
                        SearchState root_st = make_search_state(pieces_copy, cpu_pl, cpu_pl);
                        MoveTriple book_mv{};
                        if (g_use_opening_book && opening_book_pick(root_st, cpu_pl, book_mv)) {
                            if (!cpu_stop.load(std::memory_order_relaxed)) {
                                std::lock_guard<EngineMutex> lk(cpu_mutex);
                                cpu_result = {true, book_mv};
                                cpu_done   = true;
                            }
                            return;
                        }
                    }
                    // Run MCTS+AB for the bulk of the time, then verify the
                    // top candidate with a final deeper SMP pass on the last 30%.
                    double mcts_time = tlimit * 0.70;
                    double verify_time = tlimit * 0.28;
                    res = mcts_ab_root_search(pieces_copy, cpu_pl,
                                              engine_mcts_ab_depth(), mcts_time, &cpu_stop);
                    // If MCTS found a move, run quick SMP verification
                    if (res.found && !cpu_stop.load(std::memory_order_relaxed)) {
                        // Hint: put MCTS best move into TT so SMP searches it first
                        // (it's already there from MCTS AB calls, so just re-search)
                        AIResult verify = smp_cpu_pick_move(pieces_copy, cpu_pl,
                                                            depth, verify_time, &cpu_stop);
                        // Prefer the deeper AB result if it finds a move
                        if (verify.found) res = verify;
                    }
                } else {
                    res = smp_cpu_pick_move(pieces_copy, cpu_pl, depth, tlimit, &cpu_stop);
                }
                if (cpu_stop.load(std::memory_order_relaxed)) return;
                std::lock_guard<EngineMutex> lk(cpu_mutex);
                if (cpu_stop.load(std::memory_order_relaxed)) return;
                cpu_result = res;
                cpu_done = true;
            } catch (...) {
                std::lock_guard<EngineMutex> lk(cpu_mutex);
                cpu_result = {false, {}};
                cpu_done = true;
            }
        };

#if COMMANDER_ENABLE_THREADS
        cpu_thread = std::thread(run_cpu_search);
#else
        // WASM-SAFE: no detached worker threads in browser runtime.
        run_cpu_search();
#endif
    }

    void check_cpu_done() {
        if (state != GameState::CPU_THINKING) return;
        std::lock_guard<EngineMutex> lk(cpu_mutex);
        if (!cpu_done) return;
        cpu_done = false;

        if (!cpu_result.found) {
            current = human_player;
            state = GameState::HUMAN_TURN;
            set_status(nullptr);
            return;
        }

        MoveTriple& m = cpu_result.move;
        Piece* piece = get_piece(m.pid);
        if (!piece) {
            current = human_player;
            state = GameState::HUMAN_TURN;
            set_status(nullptr);
            return;
        }

        selected_id = piece->id;
        execute_move(m.dc, m.dr, true);
        if (state != GameState::GAME_OVER) {
            state = GameState::HUMAN_TURN;
            set_status(nullptr);
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// FONT SYSTEM  (SDL2_ttf)
// ═══════════════════════════════════════════════════════════════════════════

static TTF_Font* g_fnt_sm   = nullptr;  //  9pt  coords, log, small labels
static TTF_Font* g_fnt_md   = nullptr;  // 11pt  panel body
static TTF_Font* g_fnt_lg   = nullptr;  // 14pt  status bar text
static TTF_Font* g_fnt_xl   = nullptr;  // 20pt  title / game-over
static TTF_Font* g_fnt_btn  = nullptr;  // 12pt  buttons
static TTF_Font* g_fnt_head = nullptr;  // 12pt  panel section headings (bold)

static bool init_fonts() {
    if (TTF_Init() != 0) return false;

    // Bold monospace (coords, log)
    const char* mono[] = {
        // macOS — system fonts
        "/System/Library/Fonts/Supplemental/CourierNewBold.ttf",
        "/Library/Fonts/Courier New Bold.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        // macOS — Homebrew
        "/opt/homebrew/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/local/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Bold.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-B.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf",
        // Windows
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\courbd.ttf",
        nullptr
    };
    // Bold sans-serif (headings, status, title)
    const char* sans[] = {
        // macOS
        "/System/Library/Fonts/HelveticaNeue.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/Library/Fonts/Arial Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        // macOS — Homebrew
        "/opt/homebrew/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/local/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        // Linux
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        // Windows
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        nullptr
    };

    auto try_load = [](const char** list, int pt) -> TTF_Font* {
        for (int i = 0; list[i]; i++) {
            TTF_Font* f = TTF_OpenFont(list[i], pt);
            if (f) return f;
        }
        return nullptr;
    };
    auto load2 = [&](const char** a, const char** b, int pt) -> TTF_Font* {
        TTF_Font* f = try_load(a, pt);
        return f ? f : try_load(b, pt);
    };

    g_fnt_sm   = load2(mono, sans, 9);
    g_fnt_md   = load2(mono, sans, 11);
    g_fnt_lg   = load2(sans, mono, 14);
    g_fnt_xl   = load2(sans, mono, 20);
    g_fnt_btn  = load2(sans, mono, 12);
    g_fnt_head = load2(sans, mono, 11);
    return true;
}

// ── Text drawing helpers ──────────────────────────────────────────────────
// x/cy_ = anchor;  align: -1=left  0=center  +1=right
static void draw_text(SDL_Renderer* r, TTF_Font* fnt, const char* txt,
                      int x, int cy_, SDL_Color c, int align = 0) {
    if (!fnt) return;
    if (!txt || !*txt) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(fnt, txt, c);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    int tw = s->w, th = s->h;
    SDL_FreeSurface(s);
    if (!t) return;
    int dx = (align == 0) ? x - tw/2 : (align < 0 ? x : x - tw);
    SDL_Rect dst = {dx, cy_ - th/2, tw, th};
    SDL_RenderCopy(r, t, nullptr, &dst);
    SDL_DestroyTexture(t);
}
static void dtc(SDL_Renderer* r, TTF_Font* f, const char* t, int x, int y, SDL_Color c)
    { draw_text(r, f, t, x, y, c,  0); }
static void dtl(SDL_Renderer* r, TTF_Font* f, const char* t, int x, int y, SDL_Color c)
    { draw_text(r, f, t, x, y, c, -1); }

// ═══════════════════════════════════════════════════════════════════════════
// PRIMITIVE HELPERS
// ═══════════════════════════════════════════════════════════════════════════

static void set_draw_color(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static void fill_rect(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_Rect rc = {x,y,w,h}; SDL_RenderFillRect(r, &rc);
}
static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_Rect rc = {x,y,w,h}; SDL_RenderDrawRect(r, &rc);
}
static void draw_circle(SDL_Renderer* r, int cx_, int cy_, int radius) {
    int x=radius, y=0, err=0;
    while (x >= y) {
        SDL_RenderDrawPoint(r,cx_+x,cy_+y); SDL_RenderDrawPoint(r,cx_-x,cy_+y);
        SDL_RenderDrawPoint(r,cx_+x,cy_-y); SDL_RenderDrawPoint(r,cx_-x,cy_-y);
        SDL_RenderDrawPoint(r,cx_+y,cy_+x); SDL_RenderDrawPoint(r,cx_-y,cy_+x);
        SDL_RenderDrawPoint(r,cx_+y,cy_-x); SDL_RenderDrawPoint(r,cx_-y,cy_-x);
        y++; if (err<=0) err+=2*y+1; if (err>0) { x--; err-=2*x+1; }
    }
}
static void fill_circle(SDL_Renderer* r, int cx_, int cy_, int radius) {
    for (int y = -radius; y <= radius; y++) {
        int dx = (int)sqrtf(float(radius*radius - y*y));
        SDL_RenderDrawLine(r, cx_-dx, cy_+y, cx_+dx, cy_+y);
    }
}
static void draw_dashed_circle(SDL_Renderer* r, int cx_, int cy_, int rad, int seg=8) {
    int n = std::max(4, (int)(2*M_PI*rad/seg));
    for (int i = 0; i < n; i += 2) {
        float a0 = 2*M_PI*i/n, a1 = 2*M_PI*(i+1)/n;
        SDL_RenderDrawLine(r,
            (int)(cx_+cosf(a0)*rad), (int)(cy_+sinf(a0)*rad),
            (int)(cx_+cosf(a1)*rad), (int)(cy_+sinf(a1)*rad));
    }
}
static void hline(SDL_Renderer* r, int x0, int y, int x1, int lw=1) {
    for (int i=0; i<lw; i++) SDL_RenderDrawLine(r, x0, y+i, x1, y+i);
}
static void vline(SDL_Renderer* r, int x, int y0, int y1, int lw=1) {
    for (int i=0; i<lw; i++) SDL_RenderDrawLine(r, x+i, y0, x+i, y1);
}

static void draw_thick_line(SDL_Renderer* r, int x0, int y0, int x1, int y1, int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
        return;
    }
    float dx = float(x1 - x0);
    float dy = float(y1 - y0);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.5f) {
        fill_circle(r, x0, y0, std::max(1, thickness / 2));
        return;
    }
    float nx = -dy / len;
    float ny = dx / len;
    int half = thickness / 2;
    for (int i = -half; i <= half; i++) {
        int ox = (int)std::lround(nx * i);
        int oy = (int)std::lround(ny * i);
        SDL_RenderDrawLine(r, x0 + ox, y0 + oy, x1 + ox, y1 + oy);
    }
}

static void draw_arrow_head(SDL_Renderer* r, int tx, int ty, float ux, float uy, int size, int thickness) {
    float base_x = tx - ux * size;
    float base_y = ty - uy * size;
    float px = -uy;
    float py = ux;
    int lx = (int)std::lround(base_x + px * (size * 0.55f));
    int ly = (int)std::lround(base_y + py * (size * 0.55f));
    int rx = (int)std::lround(base_x - px * (size * 0.55f));
    int ry = (int)std::lround(base_y - py * (size * 0.55f));
    draw_thick_line(r, tx, ty, lx, ly, thickness);
    draw_thick_line(r, tx, ty, rx, ry, thickness);
}

// ═══════════════════════════════════════════════════════════════════════════
// TITLE BANNER  — top of window, full width
// Python equivalent: tk.Label "⭐ COMMANDER CHESS — Cờ Tư Lệnh ⭐"
// ═══════════════════════════════════════════════════════════════════════════

static void draw_title(SDL_Renderer* r) {
    set_draw_color(r, C_BG);
    fill_rect(r, 0, 0, WIN_W, TITLE_H);
    // Green accent line at bottom
    set_draw_color(r, C_GREEN);
    hline(r, 0, TITLE_H-2, WIN_W, 2);
    // Subtle glow behind title
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_draw_color(r, {0x58,0xc8,0x8c,0x15});
    fill_rect(r, WIN_W/2 - 200, 0, 400, TITLE_H);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    dtc(r, g_fnt_xl,
        "COMMANDER CHESS",
        WIN_W/2, TITLE_H/2,
        {0xe8,0xed,0xf2,0xff});
}

// ═══════════════════════════════════════════════════════════════════════════
// STATUS BAR  — below title, spans full width
// Python equivalent: self.status_lbl (Consolas 12 bold, dark red/blue bg)
// ═══════════════════════════════════════════════════════════════════════════

static void draw_status(SDL_Renderer* r, const Game& g) {
    int sy = TITLE_H;

    Color bg;
    if      (g.state == GameState::GAME_OVER) bg = {0x1a,0x1c,0x0e,0xff};
    else if (g.current == "red")              bg = {0x1a,0x12,0x14,0xff};
    else                                      bg = {0x12,0x14,0x1a,0xff};
    set_draw_color(r, bg);
    fill_rect(r, 0, sy, WIN_W, STATUS_H);

    set_draw_color(r, {0x22,0x2c,0x30,0xff});
    hline(r, 0, sy + STATUS_H - 1, WIN_W, 1);

    Color dc;
    if      (g.state == GameState::GAME_OVER) dc = C_AMBER;
    else if (g.current == "red")              dc = C_RED_DOT;
    else                                      dc = C_BLUE_DOT;
    // Glow behind dot
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_draw_color(r, {dc.r, dc.g, dc.b, 0x30});
    fill_circle(r, 24, sy + STATUS_H/2, 16);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    set_draw_color(r, dc);
    fill_circle(r, 24, sy + STATUS_H/2, 9);

    SDL_Color tc;
    if      (g.state == GameState::GAME_OVER) tc = {0xfb,0xbf,0x24,0xff};
    else if (g.current == "red")              tc = {0xf0,0x90,0x90,0xff};
    else                                      tc = {0x90,0x90,0xf0,0xff};

    std::string msg = (g.state == GameState::GAME_OVER)
        ? "  GAME OVER  -  " + g.win_msg
        : "  " + g.status_msg;
    dtl(r, g_fnt_lg, msg.c_str(), 44, sy + STATUS_H/2, tc);
}

// ═══════════════════════════════════════════════════════════════════════════
// BOARD  — rendered into viewport at y=(TITLE_H+STATUS_H), x=0, w=BW, h=BH
// ═══════════════════════════════════════════════════════════════════════════

static void draw_board(SDL_Renderer* r) {
    // Cell backgrounds
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            int x0 = cx(col)-CELL/2, y0 = cy(row)-CELL/2;
            Color fill;
            if (row==5||row==6 || is_sea(col, row)) fill = C_RIVER;
            else                fill = C_LAND;
            set_draw_color(r, fill);
            fill_rect(r, x0, y0, CELL, CELL);
        }
    }

    // River band (rows 5-6, full width)
    int ry0 = cy(6)-CELL/2, ry1 = cy(5)+CELL/2;
    set_draw_color(r, {0x88,0xd0,0xf0,0xff});
    fill_rect(r, cx(0)-CELL/2, ry0, (cx(10)+CELL/2)-(cx(0)-CELL/2), ry1-ry0);
    dtc(r, g_fnt_sm, "~ ~ ~  RIVER / SONG  ~ ~ ~",
        (cx(0)+cx(10))/2, (ry0+ry1)/2, {0x1a,0x55,0x80,0xff});

    // Keep the sea-river connector strip (col 2..2.5, row 4.5..6.5) blue.
    int seam_x0 = cx(2);
    int seam_y0 = ry0;
    int seam_h  = ry1 - ry0;
    set_draw_color(r, C_RIVER);
    fill_rect(r, seam_x0, seam_y0, CELL/2, seam_h);

    // Reef bridges at cols 5 and 7
    for (int rc : {5,7}) {
        int bx2 = cx(rc);
        set_draw_color(r, {0xc0,0xe8,0xf8,0xff});
        fill_rect(r, bx2-12, ry0+3, 24, ry1-ry0-6);
        set_draw_color(r, {0x44,0x88,0xaa,0xff});
        draw_rect(r, bx2-12, ry0+3, 24, ry1-ry0-6);
        // Python uses ≡ (U+2261); we use = as ASCII fallback
        dtc(r, g_fnt_md, "=", bx2, (ry0+ry1)/2, {0x22,0x66,0xaa,0xff});
    }

    // Grid lines — thicker on river rows
    for (int row = 0; row < ROWS; row++) {
        set_draw_color(r, C_GRID);
        hline(r, cx(0), cy(row), cx(COLS-1), (row==5||row==6) ? 2 : 1);
    }
    for (int col = 0; col < COLS; col++) {
        set_draw_color(r, C_GRID);
        vline(r, cx(col), cy(0), cy(ROWS-1), 1);
    }

    // Outer border 3px thick
    set_draw_color(r, {0x5a,0x4a,0x20,0xff});
    for (int i = 0; i < 3; i++) {
        SDL_Rect b = {cx(0)-CELL/2-i, cy(ROWS-1)-CELL/2-i,
                      (cx(COLS-1)+CELL/2)-(cx(0)-CELL/2) + 2*i,
                      (cy(0)+CELL/2)-(cy(ROWS-1)-CELL/2) + 2*i};
        SDL_RenderDrawRect(r, &b);
    }

    // Column numbers along bottom (Python: cx(col), cy(0)+CELL//2+14)
    SDL_Color coord = {0x6a,0x5a,0x30,0xff};
    for (int col = 0; col < COLS; col++) {
        char buf[4]; snprintf(buf,4,"%d",col);
        dtc(r, g_fnt_sm, buf, cx(col), cy(0)+CELL/2+16, coord);
    }
    // Row numbers along left (Python: cx(0)-CELL//2-12, cy(row))
    for (int row = 0; row < ROWS; row++) {
        char buf[4]; snprintf(buf,4,"%d",row);
        dtc(r, g_fnt_sm, buf, cx(0)-CELL/2-14, cy(row), coord);
    }

    // Territory labels
    dtc(r, g_fnt_sm, "RED TERRITORY",  cx(6), cy(2), {0xd0,0x90,0x90,0xff});
    dtc(r, g_fnt_sm, "BLUE TERRITORY", cx(6), cy(9), {0x90,0x90,0xd0,0xff});

}

static void draw_last_move_trail(SDL_Renderer* r, const Game& g) {
    int from_c = 0, from_r = 0, to_c = 0, to_r = 0;
    bool is_capture = false;
    std::string player = "red";
    bool have = false;
    if (g.is_reviewing()) {
        int mv = g.review_index - 1;
        if (mv >= 0 && mv < (int)g.move_records.size()) {
            const MoveRecord& rec = g.move_records[mv];
            from_c = rec.from_c;
            from_r = rec.from_r;
            to_c = rec.to_c;
            to_r = rec.to_r;
            is_capture = rec.capture;
            player = rec.player;
            have = true;
        }
    } else if (g.has_last_move) {
        from_c = g.last_from_c;
        from_r = g.last_from_r;
        to_c = g.last_to_c;
        to_r = g.last_to_r;
        is_capture = g.last_move_capture;
        player = g.last_move_player;
        have = true;
    }
    if (!have) return;

    int x0 = cx(from_c);
    int y0 = cy(from_r);
    int x1 = cx(to_c);
    int y1 = cy(to_r);
    Color main = (player == "red")
        ? Color{0xff,0x7a,0x7a,0xd8}
        : Color{0x7a,0xa0,0xff,0xd8};
    Color glow = is_capture
        ? Color{0xff,0xc0,0x55,0xb0}
        : Color{0xa8,0xe8,0xff,0x8a};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_draw_color(r, glow);
    draw_thick_line(r, x0, y0, x1, y1, 10);
    set_draw_color(r, main);
    draw_thick_line(r, x0, y0, x1, y1, 4);

    float dx = float(x1 - x0);
    float dy = float(y1 - y0);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
        float ux = dx / len;
        float uy = dy / len;
        int tx = (int)std::lround(x1 - ux * (PIECE_R - 2));
        int ty = (int)std::lround(y1 - uy * (PIECE_R - 2));
        draw_arrow_head(r, tx, ty, ux, uy, 14, 4);
    }

    set_draw_color(r, {0xff,0xff,0xff,0x70});
    fill_circle(r, x0, y0, 6);
    set_draw_color(r, main);
    draw_circle(r, x0, y0, PIECE_R - 6);
    draw_circle(r, x1, y1, PIECE_R - 5);
    if (is_capture) {
        set_draw_color(r, {0xff,0x66,0x66,0xff});
        draw_dashed_circle(r, x1, y1, PIECE_R + 2, 6);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_highlights(SDL_Renderer* r, const Game& g) {
    if (g.is_reviewing()) return;

    const PieceList& board = g.board_for_render();

    if (g.state == GameState::HUMAN_TURN &&
        g.current == g.human_player &&
        g.selected_id < 0) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (const auto& p : board) {
            if (p.carrier_id >= 0) continue;
            if (p.player != g.human_player) continue;
            auto mvs = get_moves(p, board);
            if (mvs.empty()) continue;
            set_draw_color(r, {0xf5,0xc8,0x42,0x42});
            fill_circle(r, cx(p.col), cy(p.row), PIECE_R + 8);
            set_draw_color(r, {0xf5,0xc8,0x42,0xb0});
            draw_dashed_circle(r, cx(p.col), cy(p.row), PIECE_R + 8, 8);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    if (g.selected_id >= 0) {
        const Piece* sel = nullptr;
        for (const auto& p : board) if (p.id==g.selected_id) { sel=&p; break; }
        if (sel) {
            set_draw_color(r, C_SEL);
            draw_dashed_circle(r, cx(sel->col), cy(sel->row), PIECE_R+6, 7);
            draw_dashed_circle(r, cx(sel->col), cy(sel->row), PIECE_R+7, 7);
        }
    }
    for (auto& m : g.valid_moves) {
        int x=cx(m.first), y=cy(m.second);
        const Piece* t = piece_at_c(board, m.first, m.second);
        if (t && t->player != g.current) {
            set_draw_color(r, C_CAPTURE);
            draw_dashed_circle(r, x, y, PIECE_R-2, 6);
            draw_dashed_circle(r, x, y, PIECE_R-1, 6);
        } else {
            SDL_SetRenderDrawColor(r, C_MOVE.r, C_MOVE.g, C_MOVE.b, 0xaa);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            fill_circle(r, x, y, 9);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
    }
}

static void draw_pieces(SDL_Renderer* r, const Game& g) {
    const PieceList& board = g.board_for_render();
    std::function<int(int)> stack_size = [&](int carrier_id) -> int {
        int n = 0;
        for (const auto& p : board) {
            if (p.carrier_id != carrier_id) continue;
            n += 1;
            n += stack_size(p.id);
        }
        return n;
    };

    for (const auto& p : board) {
        if (p.carrier_id >= 0) continue;
        int x=cx(p.col), y=cy(p.row);
        std::string key = p.kind + "_" + p.player;
        if (p.hero) {
            set_draw_color(r, C_HERO_RING);
            draw_dashed_circle(r, x, y, PIECE_R+4, 5);
            draw_dashed_circle(r, x, y, PIECE_R+5, 5);
        }
        auto it = g_textures.find(key);
        if (it != g_textures.end() && it->second) {
            SDL_Rect dst = {x-25, y-25, 50, 50};
            SDL_RenderCopy(r, it->second, nullptr, &dst);
        } else {
            Color pc = p.player=="red"
                ? Color{0xcc,0x33,0x33,0xff}
                : Color{0x22,0x44,0xcc,0xff};
            set_draw_color(r, pc); fill_circle(r, x, y, PIECE_R);
            set_draw_color(r, {0xff,0xff,0xff,0xff}); draw_circle(r, x, y, PIECE_R);
            dtc(r, g_fnt_sm, p.kind.c_str(), x, y, {0xff,0xff,0xff,0xff});
        }
        if (p.hero)
            dtc(r, g_fnt_sm, "*", x+PIECE_R, y-PIECE_R+2, {0xff,0xee,0x00,0xff});
        int carried = stack_size(p.id);
        if (carried > 0) {
            std::string lbl = "+" + std::to_string(carried);
            dtc(r, g_fnt_sm, lbl.c_str(), x+PIECE_R-2, y+PIECE_R-2, {0xff,0xcc,0x66,0xff});
        }
    }
}

struct PanelMoveLogLayout {
    bool visible = false;
    int title_y = 0;
    int box_x = 0;
    int box_y = 0;
    int box_w = 0;
    int box_h = 0;
    int row_h = 13;
    int log_start = 0;
    int rows = 0;
    SDL_Rect prev_btn{0,0,0,0};
    SDL_Rect next_btn{0,0,0,0};
};

static PanelMoveLogLayout compute_panel_move_log_layout(const Game& g) {
    PanelMoveLogLayout ml;
    int y = TITLE_H + 8;

    // YOU/CPU row + separator
    y += 16; y += 8;
    // Difficulty section
    y += 18; // header spacing
    y += 18; y += 8; // radio row + separator
    // NEXT STEP section
    y += 16;          // header spacing
    y += 14 * 3;      // three guidance lines
    if (g.has_last_move) y += 14;
    y += 8;           // separator spacing
    // HIGHLIGHT LEGEND
    y += 15;
    y += 13 + 13 + 13 + 14;
    y += 8;           // separator spacing
    // PIECE GUIDE
    y += 18; y += 5;
    for (int i = 0; i < 11; i++) {
        y += 22;
        if (y > WIN_H - 140) break;
    }
    y += 8;           // separator spacing
    // WIN CONDITIONS
    y += 16;
    y += 16;
    for (int i = 0; i < 4; i++) {
        y += 16;
        if (y > WIN_H - 90) break;
    }
    y += 6;           // separator spacing
    // HERO NOTE
    if (y < WIN_H - 70) {
        y += 14;
        y += 14;
        y += 6;
    }

    if (y >= WIN_H - 50) return ml;

    ml.visible = true;
    ml.title_y = y;
    int nav_y = y - 8;
    ml.next_btn = SDL_Rect{BW + PANEL_W - 28, nav_y, 18, 14};
    ml.prev_btn = SDL_Rect{BW + PANEL_W - 50, nav_y, 18, 14};
    int text_y = y + 16;
    ml.box_x = BW + 6;
    ml.box_y = text_y;
    ml.box_w = PANEL_W - 12;
    ml.box_h = WIN_H - text_y - 6;
    if (ml.box_h <= 10) return ml;

    int rows_fit = 0;
    for (int yy = text_y; yy + 4 < WIN_H - 8; yy += ml.row_h) rows_fit++;
    int tail = std::min(14, rows_fit);
    if (g.is_reviewing()) {
        int focus = std::max(0, g.review_index - 1);
        int max_start = std::max(0, (int)g.move_log.size() - tail);
        int centered = focus - tail / 2;
        ml.log_start = std::max(0, std::min(centered, max_start));
    } else {
        ml.log_start = std::max(0, (int)g.move_log.size() - tail);
    }
    ml.rows = std::min((int)g.move_log.size() - ml.log_start, rows_fit);
    return ml;
}

static int move_log_nav_from_panel_click(const Game& g, int mx, int my) {
    PanelMoveLogLayout ml = compute_panel_move_log_layout(g);
    if (!ml.visible) return 0;
    auto inside = [&](const SDL_Rect& rc) {
        return mx >= rc.x && mx < rc.x + rc.w && my >= rc.y && my < rc.y + rc.h;
    };
    bool prev_enabled = g.state_history.size() > 1 && (!g.is_reviewing() || g.review_index > 0);
    bool next_enabled = g.is_reviewing();
    if (prev_enabled && inside(ml.prev_btn)) return -1;
    if (next_enabled && inside(ml.next_btn)) return +1;
    return 0;
}

static int move_log_index_from_panel_click(const Game& g, int mx, int my) {
    PanelMoveLogLayout ml = compute_panel_move_log_layout(g);
    if (!ml.visible || ml.rows <= 0) return -1;
    if (mx < ml.box_x || mx >= ml.box_x + ml.box_w ||
        my < ml.box_y || my >= ml.box_y + ml.box_h) return -1;
    int row = (my - ml.box_y) / ml.row_h;
    if (row < 0 || row >= ml.rows) return -1;
    return ml.log_start + row;
}

// ═══════════════════════════════════════════════════════════════════════════
// SIDE PANEL  — x=BW..BW+PANEL_W, y=0..WIN_H
// Matches Python _build_panel() layout
// ═══════════════════════════════════════════════════════════════════════════

static void draw_panel(SDL_Renderer* r, const Game& g) {
    int px = BW;
    int mid = px + PANEL_W/2;

    // Background
    set_draw_color(r, C_PANEL);
    fill_rect(r, px, 0, PANEL_W, WIN_H);
    // Left border — subtle green
    set_draw_color(r, {0x58,0xc8,0x8c,0x33});
    vline(r, px, 0, WIN_H, 1);

    SDL_Color green_hd = {0x58,0xc8,0x8c,0xff};  // section headers
    SDL_Color body_txt = {0x90,0xa4,0xae,0xff};   // body text
    SDL_Color amber    = {0xfb,0xbf,0x24,0xff};   // amber highlights

    // Helper: horizontal separator line
    auto sep = [&](int y) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        set_draw_color(r, {0xff,0xff,0xff,0x0a});
        hline(r, px+10, y, px+PANEL_W-10, 1);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    };

    int y = TITLE_H + 8;

    // ── YOU / CPU info ─────────────────────────────────────────────────
    Color you_dot = (g.human_player == "red") ? C_RED_DOT : C_BLUE_DOT;
    SDL_Color you_tc = (g.human_player == "red")
        ? SDL_Color{0xf0,0x90,0x90,0xff}
        : SDL_Color{0x90,0xb0,0xf0,0xff};
    const char* you_lbl = (g.human_player == "red") ? "YOU = RED" : "YOU = BLUE";

    Color cpu_dot = (g.cpu_player == "red") ? C_RED_DOT : C_BLUE_DOT;
    SDL_Color cpu_tc = (g.cpu_player == "red")
        ? SDL_Color{0xf0,0x90,0x90,0xff}
        : SDL_Color{0x90,0xb0,0xf0,0xff};
    const char* cpu_lbl = (g.cpu_player == "red") ? "CPU = RED" : "CPU = BLUE";

    set_draw_color(r, you_dot);
    fill_circle(r, px+12, y, 5);
    dtl(r, g_fnt_sm, you_lbl, px+24, y, you_tc);

    set_draw_color(r, cpu_dot);
    fill_circle(r, mid+10, y, 5);
    dtl(r, g_fnt_sm, cpu_lbl, mid+22, y, cpu_tc);

    y += 16; sep(y); y += 8;

    // ── CPU DIFFICULTY ────────────────────────────────────────────────────
    dtl(r, g_fnt_head, "DIFFICULTY", px+8, y, green_hd);
    y += 18;

    const char* dlbl[] = {"Beginner","Intermediate","Expert"};
    int rbx = px+10;
    for (int i = 0; i < 3; i++) {
        bool sel = (g.difficulty == i);
        // Pill highlight when selected
        if (sel) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            set_draw_color(r, {0x58,0xc8,0x8c,0x20});
            fill_rect(r, rbx-2, y-6, (i==1?90:70), 14);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
        set_draw_color(r, sel ? C_GREEN : Color{0x44,0x55,0x44,0xff});
        fill_circle(r, rbx+4, y+1, sel ? 4 : 3);
        SDL_Color tc = sel ? green_hd : body_txt;
        dtl(r, g_fnt_sm, dlbl[i], rbx+12, y+1, tc);
        rbx += (i==0 ? 72 : (i==1 ? 90 : 60));
    }
    y += 18; sep(y); y += 8;

    // ── NEXT STEP (beginner guidance) ──────────────────────────────────
    dtl(r, g_fnt_head, "NEXT STEP", px+8, y, green_hd);
    y += 16;
    std::string tip1, tip2, tip3;
    if (g.state == GameState::GAME_OVER) {
        tip1 = "Click NEW GAME to restart.";
        tip2 = "Pick mode + side and start again.";
        tip3 = "Try a lower difficulty first.";
    } else if (g.state == GameState::CPU_THINKING) {
        tip1 = "CPU is thinking...";
        tip2 = "Watch the trail for the last move.";
        tip3 = "Plan your next capture threat.";
    } else if (g.selected_id < 0) {
        tip1 = "Click a glowing piece to select it.";
        tip2 = "Green dots = where it can move.";
        tip3 = "Red rings = captures you can make.";
    } else if (g.valid_moves.empty()) {
        tip1 = "This piece can't move right now.";
        tip2 = "Click another highlighted piece.";
        tip3 = "Keep your Commander safe!";
    } else {
        tip1 = "Click a green dot to move there.";
        tip2 = "Red rings mean you can capture!";
        tip3 = "Don't leave your Commander exposed.";
    }
    dtl(r, g_fnt_sm, tip1.c_str(), px+8, y, body_txt); y += 14;
    dtl(r, g_fnt_sm, tip2.c_str(), px+8, y, body_txt); y += 14;
    dtl(r, g_fnt_sm, tip3.c_str(), px+8, y, body_txt); y += 14;
    if (g.has_last_move) {
        std::string lm = std::string("Last: ") +
            (g.last_move_player == "red" ? "R " : "B ") +
            "(" + std::to_string(g.last_from_c) + "," + std::to_string(g.last_from_r) + ")" +
            "->(" + std::to_string(g.last_to_c) + "," + std::to_string(g.last_to_r) + ")" +
            (g.last_move_capture ? " x" : "");
        dtl(r, g_fnt_sm, lm.c_str(), px+8, y, amber);
        y += 14;
    }
    sep(y); y += 8;

    // ── HIGHLIGHT LEGEND ───────────────────────────────────────────────
    dtl(r, g_fnt_head, "LEGEND", px+8, y, green_hd);
    y += 15;
    dtl(r, g_fnt_sm, "Yellow ring = selected unit", px+8, y, body_txt); y += 13;
    dtl(r, g_fnt_sm, "Green dot = legal move", px+8, y, body_txt); y += 13;
    dtl(r, g_fnt_sm, "Red ring = capture move", px+8, y, body_txt); y += 13;
    dtl(r, g_fnt_sm, "Trail arrow = previous move", px+8, y, body_txt); y += 14;
    sep(y); y += 8;

    // ── PIECE GUIDE ───────────────────────────────────────────────────
    dtl(r, g_fnt_head, "PIECE GUIDE", px+8, y, green_hd);
    y += 18; sep(y); y += 5;

    static const std::pair<const char*,const char*> guide[] = {
        {"C",  "Commander  PROTECT!"},
        {"H",  "HQ  immobile base"},
        {"In", "Infantry  1 sq fwd"},
        {"M",  "Militia  1 sq +diag"},
        {"T",  "Tank  2 sq straight"},
        {"E",  "Engineer  carries"},
        {"A",  "Artillery  range 3"},
        {"Aa", "Anti-Air  shoots air"},
        {"Ms", "Missile  range 2"},
        {"Af", "Air Force  flies! r4"},
        {"N",  "Navy  sea power r3"},
    };
    for (auto& kv : guide) {
        auto it = g_textures_small.find(std::string(kv.first) + "_red");
        if (it != g_textures_small.end() && it->second) {
            SDL_Rect dst = {px+6, y, 20, 20};
            SDL_RenderCopy(r, it->second, nullptr, &dst);
        } else {
            set_draw_color(r, {0xcc,0x33,0x33,0xff});
            fill_circle(r, px+16, y+10, 8);
        }
        dtl(r, g_fnt_sm, kv.second, px+30, y+10, body_txt);
        y += 22;
        if (y > WIN_H - 140) break;
    }
    sep(y); y += 8;

    // ── WIN CONDITIONS ────────────────────────────────────────────────────
    dtl(r, g_fnt_head, "HOW TO WIN", px+8, y, amber);
    y += 16;
    std::string mode_line = std::string("Mode: ") + game_mode_name(g.selected_mode);
    dtl(r, g_fnt_sm, mode_line.c_str(), px+8, y, {0xfb,0xbf,0x24,0xbb});
    y += 16;
    const char* wins[] = {
        "Capture enemy Commander",
        "Destroy all 2 Navy",
        "Destroy all 2 Air Force",
        "Destroy Arty+Tank+Infantry",
    };
    for (auto* w : wins) {
        dtl(r, g_fnt_sm, w, px+8, y, body_txt);
        y += 16;
        if (y > WIN_H-90) break;
    }
    sep(y); y += 6;

    // ── HERO NOTE ─────────────────────────────────────────────────────────
    if (y < WIN_H - 70) {
        dtl(r, g_fnt_sm, "Hero: checks opponent Commander", px+8, y, {0xfb,0xbf,0x24,0x99});
        y += 14;
        dtl(r, g_fnt_sm, "  +1 range, diagonal, Air=stealth", px+8, y, {0xfb,0xbf,0x24,0x77});
        y += 14; sep(y); y += 6;
    }

    // ── MOVE LOG (click a row to review that move) ───────────────────────
    PanelMoveLogLayout ml = compute_panel_move_log_layout(g);
    if (ml.visible) {
        const char* log_title = g.is_reviewing()
            ? "MOVE LOG  [click row or < > arrows]"
            : "MOVE LOG  [click row or < to review]";
        dtl(r, g_fnt_head, log_title, px+8, ml.title_y, green_hd);

        bool prev_enabled = g.state_history.size() > 1 && (!g.is_reviewing() || g.review_index > 0);
        bool next_enabled = g.is_reviewing();
        auto draw_nav_btn = [&](const SDL_Rect& rc, const char* lbl, bool enabled) {
            set_draw_color(r, enabled ? Color{0x1a,0x28,0x22,0xff} : Color{0x18,0x1c,0x22,0xff});
            fill_rect(r, rc.x, rc.y, rc.w, rc.h);
            set_draw_color(r, enabled ? Color{0x58,0xc8,0x8c,0x66} : Color{0x44,0x44,0x44,0xff});
            draw_rect(r, rc.x, rc.y, rc.w, rc.h);
            dtc(r, g_fnt_sm, lbl, rc.x + rc.w/2, rc.y + rc.h/2,
                enabled ? SDL_Color{0x58,0xc8,0x8c,0xff} : SDL_Color{0x55,0x55,0x55,0xff});
        };
        draw_nav_btn(ml.prev_btn, "<", prev_enabled);
        draw_nav_btn(ml.next_btn, ">", next_enabled);

        if (ml.box_h > 10) {
            set_draw_color(r, {0x0d,0x11,0x17,0xff});
            fill_rect(r, ml.box_x, ml.box_y, ml.box_w, ml.box_h);
            set_draw_color(r, {0x22,0x2c,0x30,0xff});
            draw_rect(r, ml.box_x, ml.box_y, ml.box_w, ml.box_h);
        }

        int selected_move = g.is_reviewing() ? (g.review_index - 1) : -1;
        int line_y = ml.box_y;
        for (int row = 0; row < ml.rows; row++) {
            int i = ml.log_start + row;
            bool selected = (i == selected_move);
            if (selected) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                set_draw_color(r, {0x58,0xc8,0x8c,0x22});
                fill_rect(r, ml.box_x + 1, line_y, ml.box_w - 2, ml.row_h);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }

            bool you = !g.move_log[i].empty() && g.move_log[i][0]=='Y';
            SDL_Color tc = selected
                ? SDL_Color{0x58,0xc8,0x8c,0xff}
                : (you ? SDL_Color{0xf0,0x90,0x90,0xff}
                       : SDL_Color{0x90,0xb0,0xf0,0xff});
            std::string e = std::to_string(i + 1) + ". " + g.move_log[i];
            if (e.size() > 36) e = e.substr(0,35) + "..";
            dtl(r, g_fnt_sm, e.c_str(), px+10, line_y+7, tc);
            line_y += ml.row_h;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GAME-OVER OVERLAY
// ═══════════════════════════════════════════════════════════════════════════

static void draw_game_over(SDL_Renderer* r, const Game& g) {
    int board_y = TITLE_H + STATUS_H;
    // Full board dim
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0x0d, 0x11, 0x17, 200);
    fill_rect(r, 0, board_y, BW, BH);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // Centered card
    int ow=BW-60, oh=160;
    int ox=(BW-ow)/2, oy=board_y+(BH-oh)/2;

    set_draw_color(r, {0x14,0x1e,0x28,0xff});
    fill_rect(r, ox, oy, ow, oh);

    // Green border
    set_draw_color(r, C_GREEN);
    for (int i=0; i<2; i++) draw_rect(r, ox+i, oy+i, ow-2*i, oh-2*i);

    // Check if player won
    bool player_won = (g.win_msg.find("RED") != std::string::npos && g.human_player == "red") ||
                      (g.win_msg.find("BLUE") != std::string::npos && g.human_player == "blue");

    const char* title = player_won ? "VICTORY!" : "GAME OVER";
    SDL_Color title_c = player_won
        ? SDL_Color{0x58,0xc8,0x8c,0xff}
        : SDL_Color{0xfb,0xbf,0x24,0xff};
    dtc(r, g_fnt_xl, title, BW/2, oy+36, title_c);
    dtc(r, g_fnt_lg, g.win_msg.c_str(), BW/2, oy+74, {0xe8,0xed,0xf2,0xff});

    const char* sub = player_won ? "Outstanding strategy, Commander!" : "Better luck next time, Commander.";
    dtc(r, g_fnt_md, sub, BW/2, oy+105, {0x90,0xa4,0xae,0xff});
    dtc(r, g_fnt_sm, "Click NEW GAME to play again", BW/2, oy+135, {0x78,0x90,0x9c,0x99});
}

static void draw_button(SDL_Renderer* r, int x, int y, int w, int h,
                        const char* lbl, SDL_Color tc, bool hover=false, bool disabled=false);

static SDL_Rect mode_menu_card_rect(int idx) {
    const int top = TITLE_H + STATUS_H + 140;
    const int margin = 34;
    const int gap = 18;
    const int w = (WIN_W - margin * 2 - gap) / 2;
    const int h = 132;
    int row = idx / 2;
    int col = idx % 2;
    return SDL_Rect{margin + col * (w + gap), top + row * (h + gap), w, h};
}

static SDL_Rect mode_menu_start_rect() {
    SDL_Rect c2 = mode_menu_card_rect(2);
    SDL_Rect c3 = mode_menu_card_rect(3);
    int y = std::max(c2.y + c2.h, c3.y + c3.h) + 72;
    return SDL_Rect{WIN_W / 2 - 120, y, 240, 40};
}

static SDL_Rect mode_menu_side_rect(int idx) {
    SDL_Rect s = mode_menu_start_rect();
    const int w = 180;
    const int h = 34;
    const int gap = 20;
    const int total = w * 2 + gap;
    const int x0 = WIN_W / 2 - total / 2;
    const int y = s.y - 52;
    return SDL_Rect{x0 + idx * (w + gap), y, w, h};
}

static SDL_Rect mode_menu_quit_rect() {
    SDL_Rect s = mode_menu_start_rect();
    return SDL_Rect{WIN_W / 2 - 60, s.y + s.h + 10, 120, 28};
}

static GameMode mode_for_menu_index(int idx) {
    switch (idx) {
    case 1: return GameMode::MARINE_BATTLE;
    case 2: return GameMode::AIR_BATTLE;
    case 3: return GameMode::LAND_BATTLE;
    case 0:
    default: return GameMode::FULL_BATTLE;
    }
}

static void draw_mode_selection_screen(SDL_Renderer* r, const Game& g) {
    int top = TITLE_H + STATUS_H;
    set_draw_color(r, C_BG);
    fill_rect(r, 0, top, WIN_W, WIN_H - top);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 9; i++) {
        uint8_t a = (uint8_t)(4 + i * 3);
        set_draw_color(r, {0x14,0x1e,0x28,a});
        int band_h = (WIN_H - top) / 9;
        fill_rect(r, 0, top + i * band_h, WIN_W, band_h);
    }
    set_draw_color(r, {0x58,0xc8,0x8c,0x10});
    fill_circle(r, WIN_W / 2, top + 34, 180);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    dtc(r, g_fnt_xl, "Choose Battle Mode", WIN_W / 2, top + 24, {0xe8,0xed,0xf2,0xff});
    dtc(r, g_fnt_md, "Step 1: mode   Step 2: side   Step 3: start",
        WIN_W / 2, top + 52, {0x90,0xa4,0xae,0xff});

    auto draw_step_chip = [&](int idx, const char* title, const char* detail, bool done, bool active) {
        const int w = 300;
        const int h = 42;
        const int gap = 16;
        const int total = w * 3 + gap * 2;
        int x = WIN_W / 2 - total / 2 + idx * (w + gap);
        int y = top + 78;
        Color bg = done ? Color{0x14,0x22,0x1a,0xff}
                        : (active ? Color{0x1a,0x23,0x32,0xff} : Color{0x14,0x18,0x20,0xff});
        Color edge = done ? Color{0x58,0xc8,0x8c,0xff}
                          : (active ? Color{0x58,0xc8,0x8c,0x55} : Color{0x33,0x3c,0x44,0xff});
        set_draw_color(r, bg);
        fill_rect(r, x, y, w, h);
        set_draw_color(r, edge);
        draw_rect(r, x, y, w, h);

        set_draw_color(r, done ? Color{0x58,0xc8,0x8c,0xff}
                               : (active ? Color{0xfb,0xbf,0x24,0xff} : Color{0x55,0x55,0x55,0xff}));
        fill_circle(r, x + 13, y + h/2, 7);
        dtl(r, g_fnt_sm, title, x + 26, y + 13,
            done ? SDL_Color{0x58,0xc8,0x8c,0xff}
                 : (active ? SDL_Color{0xe8,0xed,0xf2,0xff} : SDL_Color{0x78,0x90,0x9c,0xff}));
        dtl(r, g_fnt_sm, detail, x + 26, y + 28, {0x90,0xa4,0xae,0xff});
    };

    bool side_done = g.menu_side_chosen;
    draw_step_chip(0, "STEP 1  MODE", game_mode_name(g.selected_mode), true, false);
    draw_step_chip(1, "STEP 2  SIDE",
                   side_done ? (g.menu_side_choice == "red" ? "You are RED" : "You are BLUE")
                             : "Choose RED or BLUE",
                   side_done, !side_done);
    draw_step_chip(2, "STEP 3  START",
                   side_done ? "Ready to begin" : "Locked until side is set",
                   false, side_done);

    struct CardText {
        const char* title;
        const char* l1;
        const char* l2;
        const char* l3;
    };
    static const CardText cards[] = {
        {"FULL BATTLE",   "Win by capturing",      "the enemy Commander.",     "Default full ruleset."},
        {"MARINE BATTLE", "Win if enemy has",      "no Navy (N) left.",        "Commander capture also wins."},
        {"AIR BATTLE",    "Win if enemy has",      "no Air Force (Af) left.",  "Commander capture also wins."},
        {"LAND BATTLE",   "Win if enemy has",      "no T, In, and A left.",    "Commander capture also wins."},
    };

    for (int i = 0; i < 4; i++) {
        SDL_Rect rc = mode_menu_card_rect(i);
        GameMode mode = mode_for_menu_index(i);
        bool sel = (g.selected_mode == mode);

        set_draw_color(r, sel ? Color{0x14,0x22,0x1a,0xff} : Color{0x14,0x18,0x20,0xff});
        fill_rect(r, rc.x, rc.y, rc.w, rc.h);
        set_draw_color(r, sel ? Color{0x58,0xc8,0x8c,0xff} : Color{0x33,0x3c,0x44,0xff});
        draw_rect(r, rc.x, rc.y, rc.w, rc.h);
        if (sel) draw_rect(r, rc.x+1, rc.y+1, rc.w-2, rc.h-2);

        dtc(r, g_fnt_lg, cards[i].title, rc.x + rc.w / 2, rc.y + 24,
            sel ? SDL_Color{0x58,0xc8,0x8c,0xff} : SDL_Color{0x90,0xa4,0xae,0xff});
        dtl(r, g_fnt_md, cards[i].l1, rc.x + 12, rc.y + 52, {0xb0,0xbe,0xc5,0xff});
        dtl(r, g_fnt_md, cards[i].l2, rc.x + 12, rc.y + 74, {0xb0,0xbe,0xc5,0xff});
        dtl(r, g_fnt_sm, cards[i].l3, rc.x + 12, rc.y + 100, {0x78,0x90,0x9c,0xff});
    }

    SDL_Rect s = mode_menu_start_rect();
    SDL_Rect q = mode_menu_quit_rect();
    SDL_Rect sr = mode_menu_side_rect(0);
    SDL_Rect sb = mode_menu_side_rect(1);

    auto draw_side_btn = [&](const SDL_Rect& rc, const char* label, const std::string& side) {
        bool sel = g.menu_side_chosen && g.menu_side_choice == side;
        set_draw_color(r, sel ? Color{0x14,0x22,0x1a,0xff} : Color{0x14,0x18,0x20,0xff});
        fill_rect(r, rc.x, rc.y, rc.w, rc.h);
        Color edge = sel ? C_GREEN
                         : (side == "red" ? Color{0xdc,0x35,0x45,0x55} : Color{0x3b,0x82,0xf6,0x55});
        set_draw_color(r, edge);
        draw_rect(r, rc.x, rc.y, rc.w, rc.h);

        set_draw_color(r, side == "red" ? C_RED_DOT : C_BLUE_DOT);
        fill_circle(r, rc.x + 14, rc.y + rc.h/2, 6);
        dtc(r, g_fnt_btn, label, rc.x + rc.w/2 + 8, rc.y + rc.h/2,
            sel ? SDL_Color{0x58,0xc8,0x8c,0xff} : SDL_Color{0xb0,0xbe,0xc5,0xff});
    };

    std::string chosen_mode = std::string("Selected mode: ") + game_mode_name(g.selected_mode);
    dtc(r, g_fnt_md, chosen_mode.c_str(), WIN_W / 2, sr.y - 34, {0x58,0xc8,0x8c,0xbb});
    std::string chosen_side = g.menu_side_chosen
        ? std::string("Selected side: YOU = ") + (g.menu_side_choice == "red" ? "RED" : "BLUE")
        : "Selected side: (choose RED or BLUE)";
    dtc(r, g_fnt_sm, chosen_side.c_str(), WIN_W / 2, sr.y - 14, {0x90,0xa4,0xae,0xff});

    draw_side_btn(sr, "PLAY AS RED", "red");
    draw_side_btn(sb, "PLAY AS BLUE", "blue");

    draw_button(r, s.x, s.y, s.w, s.h, "START GAME",
                g.menu_side_chosen ? SDL_Color{0x58,0xc8,0x8c,0xff}
                                   : SDL_Color{0x55,0x55,0x55,0xff},
                false, !g.menu_side_chosen);
    draw_button(r, q.x, q.y, q.w, q.h, "Quit", {0xff,0x88,0x66,0xff});
}

// ── Button helper ─────────────────────────────────────────────────────────
static void draw_button(SDL_Renderer* r, int x, int y, int w, int h,
                        const char* lbl, SDL_Color tc, bool hover, bool disabled) {
    Color bg = disabled
        ? Color{0x18,0x1c,0x22,0xff}
        : (hover ? Color{0x1a,0x28,0x22,0xff} : Color{0x14,0x1e,0x28,0xff});
    Color edge = disabled ? Color{0x33,0x3c,0x44,0xff} : Color{0x58,0xc8,0x8c,0x77};
    set_draw_color(r, bg);
    fill_rect(r, x, y, w, h);
    set_draw_color(r, edge);
    draw_rect(r, x, y, w, h);
    dtc(r, g_fnt_btn, lbl, x+w/2, y+h/2, tc);
}

// ─────────────────────────────────────────────────────────────────────────
struct Button { SDL_Rect rect; std::string label; std::function<void()> action; };

struct SimOptions {
    bool enabled = false;
    int games = 1000;
    int seed = 1;
    int depth = 4;
    int time_ms = 50;
    int max_plies = 300;
    std::string start = "alternate"; // red | blue | alternate | random
    bool mcts = false;
};

static bool parse_i32_arg(const char* s, int& out) {
    if (!s || !*s) return false;
    std::stringstream ss(s);
    int v = 0;
    char extra = 0;
    if (!(ss >> v)) return false;
    if (ss >> extra) return false;
    out = v;
    return true;
}

static void print_usage(const char* prog) {
    std::cout
        << "Usage:\n"
        << "  " << prog << "\n"
        << "  " << prog << " [--eval_backend MODE]\n"
        << "  " << prog << " --sim [--games N] [--seed S] [--depth D] [--time_ms T] [--max_plies P] [--start MODE] [--mcts]\n"
        << "\n"
        << "General options:\n"
        << "  --eval_backend MODE    MODE: auto | cpu | webgpu   (default: auto)\n"
        << "\n"
        << "Defaults in --sim mode:\n"
        << "  --games 1000 --seed 1 --depth 4 --time_ms 50 --max_plies 300 --start alternate\n"
        << "  MODE: red | blue | alternate | random\n"
        << "  --mcts enables hybrid MCTS+AB move selection in sim mode\n";
}

static int run_headless_sim(const SimOptions& opt) {
    init_zobrist();
    tt_ensure_allocated();
    std::srand((unsigned int)opt.seed);
    std::mt19937 rng((uint32_t)opt.seed);
    bool prev_book = g_use_opening_book;
    bool prev_mcts = g_use_mcts;
    g_use_opening_book = false; // fair self-play: avoid side-specific opening-book bias
    g_use_mcts = opt.mcts;

    int red_wins = 0;
    int blue_wins = 0;
    int draws = 0;
    int started_red = 0;
    int started_blue = 0;
    int starter_wins = 0;
    int nonstarter_wins = 0;
    const double time_limit_secs = (double)opt.time_ms / 1000.0;

    auto t0 = std::chrono::steady_clock::now();

    for (int g = 0; g < opt.games; g++) {
        PieceList pieces = make_initial_pieces();
        std::string turn = "red";
        if (opt.start == "blue") turn = "blue";
        else if (opt.start == "alternate") turn = (g % 2 == 0) ? "red" : "blue";
        else if (opt.start == "random") turn = (std::uniform_int_distribution<int>(0, 1)(rng) == 0) ? "red" : "blue";
        const std::string starter = turn;
        if (starter == "red") started_red++;
        else started_blue++;
        std::vector<uint64_t> rep_history;
        push_position_history(rep_history, zobrist_hash(pieces, turn));

        std::string init_why;
        if (!validate_state_for_sim(pieces, opp(starter), &init_why)) {
            std::cerr
                << "[sim] invalid initial state"
                << " seed=" << opt.seed
                << " game=" << g
                << " starter=" << starter
                << " reason=\"" << init_why << "\"\n";
            std::abort();
        }
        bool finished = false;
        tt_clear();  // CRITICAL: purge all TT entries between games to prevent
                     //   cross-perspective value pollution (red stores "+500 good-for-red",
                     //   blue reads "+500" and misinterprets as "good-for-blue").

        for (int ply = 0; ply < opt.max_plies; ply++) {
            reset_search_tables();
            tt_clear();  // Self-play: purge TT every move since perspective alternates
            g_game_rep_history = rep_history;  // let search see game's repetition history
            AIResult r = cpu_pick_move(pieces, turn, opt.depth, time_limit_secs);
            if (!r.found) {
                draws++;
                finished = true;
                break;
            }

            pieces = apply_move(pieces, r.move.pid, r.move.dc, r.move.dr, turn);

            std::string why;
            if (!validate_state_for_sim(pieces, turn, &why)) {
                std::cerr
                    << "[sim] invalid state"
                    << " seed=" << opt.seed
                    << " game=" << g
                    << " ply=" << ply
                    << " turn=" << turn
                    << " move=(" << r.move.pid << " -> " << r.move.dc << "," << r.move.dr << ")"
                    << " reason=\"" << why << "\"\n";
                std::abort();
            }

            std::string win = check_win(pieces, turn);
            if (!win.empty()) {
                if (turn == "red") red_wins++;
                else               blue_wins++;
                if (turn == starter) starter_wins++;
                else                 nonstarter_wins++;
                finished = true;
                break;
            }

            turn = opp(turn);
            uint64_t rep_hash = zobrist_hash(pieces, turn);
            push_position_history(rep_history, rep_hash);
            if (is_threefold_repetition(rep_history, rep_hash)) {
                draws++;
                finished = true;
                break;
            }
        }

        if (!finished) draws++;
    }

    double total_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double ms_per_game = (opt.games > 0) ? (total_seconds * 1000.0 / (double)opt.games) : 0.0;
    double games_per_hour = (total_seconds > 0.0) ? ((double)opt.games * 3600.0 / total_seconds) : 0.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "SIM CONFIG: games=" << opt.games
              << " seed=" << opt.seed
              << " depth=" << opt.depth
              << " time_ms=" << opt.time_ms
              << " max_plies=" << opt.max_plies
              << " start=" << opt.start
              << " mcts=" << (opt.mcts ? 1 : 0) << "\n";
    std::cout << "EVAL BACKEND: " << eval_backend_name(active_eval_backend()) << "\n";
    std::cout << "RESULTS: red_wins=" << red_wins
              << " blue_wins=" << blue_wins
              << " draws=" << draws << "\n";
    std::cout << "STARTERS: red_started=" << started_red
              << " blue_started=" << started_blue
              << " starter_wins=" << starter_wins
              << " nonstarter_wins=" << nonstarter_wins << "\n";
    std::cout << "total seconds: " << total_seconds << "\n";
    std::cout << "ms/game: " << ms_per_game << "\n";
    std::cout << "games/hour estimate: " << games_per_hour << "\n";
    g_use_opening_book = prev_book;
    g_use_mcts = prev_mcts;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::atexit(tt_arena_release);
    SimOptions sim;
    bool saw_sim_option = false;
    std::string eval_backend_mode = "auto";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--eval_backend") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --eval_backend\n";
                print_usage(argv[0]);
                return 1;
            }
            eval_backend_mode = argv[++i];
        } else if (arg == "--sim") {
            sim.enabled = true;
        } else if (arg == "--mcts") {
            saw_sim_option = true;
            sim.mcts = true;
        } else if (arg == "--start") {
            saw_sim_option = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --start\n";
                print_usage(argv[0]);
                return 1;
            }
            sim.start = argv[++i];
            if (sim.start != "red" && sim.start != "blue" &&
                sim.start != "alternate" && sim.start != "random") {
                std::cerr << "--start must be one of: red, blue, alternate, random\n";
                return 1;
            }
        } else if (arg == "--games" || arg == "--seed" || arg == "--depth" ||
                   arg == "--time_ms" || arg == "--max_plies") {
            saw_sim_option = true;
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            }
            int v = 0;
            if (!parse_i32_arg(argv[++i], v)) {
                std::cerr << "Invalid integer for " << arg << ": " << argv[i] << "\n";
                return 1;
            }
            if (arg == "--games") {
                if (v <= 0) { std::cerr << "--games must be > 0\n"; return 1; }
                sim.games = v;
            } else if (arg == "--seed") {
                sim.seed = v;
            } else if (arg == "--depth") {
                if (v <= 0) { std::cerr << "--depth must be > 0\n"; return 1; }
                sim.depth = v;
            } else if (arg == "--time_ms") {
                if (v <= 0) { std::cerr << "--time_ms must be > 0\n"; return 1; }
                sim.time_ms = v;
            } else if (arg == "--max_plies") {
                if (v <= 0) { std::cerr << "--max_plies must be > 0\n"; return 1; }
                sim.max_plies = v;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!sim.enabled && saw_sim_option) {
        std::cerr << "Simulation options require --sim\n";
        print_usage(argv[0]);
        return 1;
    }
    std::string eval_note;
    if (!configure_eval_backend(eval_backend_mode, &eval_note)) {
        std::cerr << "Invalid value for --eval_backend: " << eval_backend_mode
                  << " (expected: auto | cpu | webgpu)\n";
        print_usage(argv[0]);
        return 1;
    }
    if (!eval_note.empty()) {
        std::cerr << "[eval] " << eval_note << "\n";
    }
    std::cerr << "[eval] active backend: " << eval_backend_name(active_eval_backend()) << "\n";
    if (sim.enabled) return run_headless_sim(sim);

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
        SDL_Quit(); return 1;
    }
    init_fonts();
    init_audio();
    init_zobrist();
    tt_ensure_allocated();

    SDL_Window* win = SDL_CreateWindow(
        "Commander Chess",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) { SDL_Log("Window failed: %s", SDL_GetError()); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { SDL_Log("Renderer failed: %s", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    load_all_textures(ren);
    Game game;
    bool running = true;

    // ── Window layout (absolute coords) ───────────────────────────────────
    //  y=0               : title bar         (TITLE_H px)
    //  y=TITLE_H         : status bar        (STATUS_H px)
    //  y=TITLE_H+STATUS_H: board             (BH px, x=0..BW)
    //  x=BW              : side panel        (PANEL_W px, full height)
    // No separate bottom button bar — buttons live at bottom of panel

    int board_top = TITLE_H + STATUS_H;

    // Clickable buttons
    // "New Game" and "Quit" sit at the very bottom of the panel
    int btn_y  = WIN_H - 38;
    int btn_w  = (PANEL_W - 24) / 2;

    // Difficulty radio-button hit areas (inside panel)
    // These are handled by checking proximity to the radio buttons drawn by draw_panel
    // Simpler: define 3 wide hit-rects spanning where the text labels are
    int diff_y = TITLE_H + 8 + 16 + 8 + 18;  // matches draw_panel layout
    int rbx0 = BW + 10;

    std::vector<Button> game_buttons = {
        // New Game button (bottom of panel, left half)
        {{BW+8,   btn_y, btn_w, 30}, "New Game", [&](){ game.open_mode_menu(); }},
        // Quit button (bottom of panel, right half)
        {{BW+8+btn_w+8, btn_y, btn_w, 30}, "Quit", [&](){ running=false; }},
        // Difficulty — wide tap targets
        {{rbx0,      diff_y, 70, 16}, "Beginner",     [&](){ game.set_difficulty(0); }},
        {{rbx0+72,   diff_y, 90, 16}, "Intermediate", [&](){ game.set_difficulty(1); }},
        {{rbx0+164,  diff_y, 60, 16}, "Expert",       [&](){ game.set_difficulty(2); }},
    };

    SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                int mx=ev.button.x, my=ev.button.y;
                bool hit = false;
                auto inside = [&](const SDL_Rect& rc) {
                    return mx>=rc.x && mx<rc.x+rc.w && my>=rc.y && my<rc.y+rc.h;
                };

                if (game.show_mode_menu) {
                    for (int i = 0; i < 4; i++) {
                        SDL_Rect rc = mode_menu_card_rect(i);
                        if (inside(rc)) {
                            game.set_game_mode(mode_for_menu_index(i));
                            hit = true;
                            break;
                        }
                    }
                    if (!hit) {
                        SDL_Rect sr = mode_menu_side_rect(0);
                        if (inside(sr)) {
                            game.set_player_side("red");
                            hit = true;
                        }
                    }
                    if (!hit) {
                        SDL_Rect sb = mode_menu_side_rect(1);
                        if (inside(sb)) {
                            game.set_player_side("blue");
                            hit = true;
                        }
                    }
                    if (!hit) {
                        SDL_Rect s = mode_menu_start_rect();
                        if (inside(s)) {
                            game.start_selected_mode_game();
                            hit = true;
                        }
                    }
                    if (!hit) {
                        SDL_Rect q = mode_menu_quit_rect();
                        if (inside(q)) {
                            running = false;
                            hit = true;
                        }
                    }
                } else {
                    for (auto& b : game_buttons) {
                        if (inside(b.rect)) {
                            b.action(); hit=true; break;
                        }
                    }
                    if (!hit && mx >= BW) {
                        int nav = move_log_nav_from_panel_click(game, mx, my);
                        if (nav < 0) {
                            game.review_prev_move();
                            hit = true;
                        } else if (nav > 0) {
                            game.review_next_move();
                            hit = true;
                        }
                    }
                    if (!hit && mx >= BW) {
                        int mv_idx = move_log_index_from_panel_click(game, mx, my);
                        if (mv_idx >= 0) {
                            game.review_move_at(mv_idx);
                            hit = true;
                        }
                    }
                    // Board click — translate to board-local coords
                    if (!hit && mx<BW && my>=board_top && my<board_top+BH)
                        game.on_click(mx, my - board_top);
                }
            }
        }
        game.check_cpu_done();

        // ── Render ────────────────────────────────────────────────────────
        set_draw_color(ren, C_BG);
        SDL_RenderClear(ren);

        // Title
        draw_title(ren);
        // Status bar
        draw_status(ren, game);
        if (game.show_mode_menu) {
            draw_mode_selection_screen(ren, game);
        } else {
            // Board (in viewport so cx/cy work in board-local coords)
            {
                SDL_Rect vp = {0, board_top, BW, BH};
                SDL_RenderSetViewport(ren, &vp);
                draw_board(ren);
                draw_last_move_trail(ren, game);
                draw_highlights(ren, game);
                draw_pieces(ren, game);
                SDL_RenderSetViewport(ren, nullptr);
            }
            // Side panel
            draw_panel(ren, game);
            // New Game / Quit buttons at bottom of panel
            draw_button(ren, game_buttons[0].rect.x, game_buttons[0].rect.y,
                             game_buttons[0].rect.w, game_buttons[0].rect.h,
                             "New Game", {0x58,0xc8,0x8c,0xff});
            draw_button(ren, game_buttons[1].rect.x, game_buttons[1].rect.y,
                             game_buttons[1].rect.w, game_buttons[1].rect.h,
                             "Quit", {0x90,0xa4,0xae,0xff});
            // Game-over overlay
            if (game.state == GameState::GAME_OVER)
                draw_game_over(ren, game);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    // Cleanup
    for (auto& kv : g_textures)       SDL_DestroyTexture(kv.second);
    for (auto& kv : g_textures_small) SDL_DestroyTexture(kv.second);
    if (g_audio_dev) SDL_CloseAudioDevice(g_audio_dev);
    if (g_fnt_sm)   TTF_CloseFont(g_fnt_sm);
    if (g_fnt_md)   TTF_CloseFont(g_fnt_md);
    if (g_fnt_lg)   TTF_CloseFont(g_fnt_lg);
    if (g_fnt_xl)   TTF_CloseFont(g_fnt_xl);
    if (g_fnt_btn)  TTF_CloseFont(g_fnt_btn);
    if (g_fnt_head) TTF_CloseFont(g_fnt_head);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
