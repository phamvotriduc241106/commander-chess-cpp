#include "engine.hpp"
#include "third_party/httplib.h"
#include "third_party/json.hpp"

#include <cstdlib>
#include <cctype>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

namespace {

std::unordered_map<std::string, commander::GameState> g_sessions;
std::mutex g_sessions_mu;

std::string make_game_id() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(rng);
    uint64_t b = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << a
        << std::setw(16) << b;
    return oss.str();
}

std::string normalize_difficulty(std::string d) {
    for (char& ch : d) ch = (char)std::tolower((unsigned char)ch);
    if (d == "easy" || d == "beginner") return "easy";
    if (d == "hard" || d == "expert") return "hard";
    return "medium";
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
    // Alternate payload support: {piece_id, to_col, to_row}
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

void set_json(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

template <typename Fn>
void register_post(httplib::Server& svr, const std::string& path, Fn&& fn) {
    svr.Post(path, fn);
    svr.Post("/api" + path, fn); // support Firebase rewrite prefix
}

} // namespace

int main() {
    httplib::Server svr;

    auto health_handler = [](const httplib::Request&, httplib::Response& res) {
        set_json(res, 200, json{{"ok", true}});
    };
    svr.Get("/health", health_handler);
    svr.Get("/api/health", health_handler);

    auto sprites_handler = [](const httplib::Request&, httplib::Response& res) {
        json sprites = json::object();
        for (const auto& kv : commander::piece_sprites()) sprites[kv.first] = kv.second;
        set_json(res, 200, json{{"sprites", sprites}});
    };
    svr.Get("/sprites", sprites_handler);
    svr.Get("/api/sprites", sprites_handler);

    register_post(svr, "/new", [](const httplib::Request& req, httplib::Response& res) {
        std::string human = "red";
        std::string game_mode = "full";
        std::string difficulty = "medium";
        if (!req.body.empty()) {
            json in = json::parse(req.body, nullptr, false);
            if (!in.is_discarded() && in.is_object() && in.contains("human_player")) {
                std::string p = in.value("human_player", "red");
                if (p == "red" || p == "blue") human = p;
            }
            if (!in.is_discarded() && in.is_object() && in.contains("game_mode")) {
                std::string m = in.value("game_mode", "full");
                if (m == "full" || m == "marine" || m == "air" || m == "land") game_mode = m;
            }
            if (!in.is_discarded() && in.is_object() && in.contains("difficulty")) {
                difficulty = normalize_difficulty(in.value("difficulty", "medium"));
            }
        }

        commander::GameState st = commander::new_game(game_mode, difficulty);
        st.human_player = human;
        st.bot_player = (human == "red") ? "blue" : "red";

        // If human chose blue, bot (red) moves first.
        if (st.current == st.bot_player) {
            commander::bot_move(st);
        }

        std::string gid = make_game_id();
        {
            std::lock_guard<std::mutex> lk(g_sessions_mu);
            g_sessions[gid] = st;
        }

        set_json(res, 200, json{{"game_id", gid}, {"state", state_to_json(commander::serialize_state(st))}});
    });

    register_post(svr, "/move", [](const httplib::Request& req, httplib::Response& res) {
        json in = json::parse(req.body, nullptr, false);
        if (in.is_discarded() || !in.is_object()) {
            set_json(res, 400, json{{"error", "invalid JSON body"}});
            return;
        }

        std::string gid = in.value("game_id", "");
        if (gid.empty()) {
            set_json(res, 400, json{{"error", "missing game_id"}});
            return;
        }

        commander::Move mv;
        if (!in.contains("move") || !parse_move(in["move"], mv)) {
            set_json(res, 400, json{{"error", "missing/invalid move"}});
            return;
        }

        std::lock_guard<std::mutex> lk(g_sessions_mu);
        auto it = g_sessions.find(gid);
        if (it == g_sessions.end()) {
            set_json(res, 404, json{{"error", "game_id not found"}});
            return;
        }

        commander::ActionStatus st = commander::apply_move(it->second, mv);
        if (!st.ok) {
            set_json(res, 400, json{{"error", st.error}});
            return;
        }

        set_json(res, 200, json{{"state", state_to_json(commander::serialize_state(it->second))}});
    });

    register_post(svr, "/bot", [](const httplib::Request& req, httplib::Response& res) {
        json in = json::parse(req.body, nullptr, false);
        if (in.is_discarded() || !in.is_object()) {
            set_json(res, 400, json{{"error", "invalid JSON body"}});
            return;
        }

        std::string gid = in.value("game_id", "");
        if (gid.empty()) {
            set_json(res, 400, json{{"error", "missing game_id"}});
            return;
        }

        std::lock_guard<std::mutex> lk(g_sessions_mu);
        auto it = g_sessions.find(gid);
        if (it == g_sessions.end()) {
            set_json(res, 404, json{{"error", "game_id not found"}});
            return;
        }

        if (it->second.game_over) {
            set_json(res, 200, json{{"state", state_to_json(commander::serialize_state(it->second))}});
            return;
        }

        if (in.contains("difficulty") && in["difficulty"].is_string()) {
            it->second.difficulty = normalize_difficulty(in.value("difficulty", "medium"));
        }

        commander::Move m = commander::bot_move(it->second);
        if (m.pid < 0) {
            set_json(res, 400, json{{"error", "bot could not find a legal move"}});
            return;
        }

        set_json(res, 200, json{{"move", move_to_json(m)}, {"state", state_to_json(commander::serialize_state(it->second))}});
    });

    register_post(svr, "/hint", [](const httplib::Request& req, httplib::Response& res) {
        json in = json::parse(req.body, nullptr, false);
        if (in.is_discarded() || !in.is_object()) {
            set_json(res, 400, json{{"error", "invalid JSON body"}});
            return;
        }

        std::string gid = in.value("game_id", "");
        if (gid.empty()) {
            set_json(res, 400, json{{"error", "missing game_id"}});
            return;
        }

        std::lock_guard<std::mutex> lk(g_sessions_mu);
        auto it = g_sessions.find(gid);
        if (it == g_sessions.end()) {
            set_json(res, 404, json{{"error", "game_id not found"}});
            return;
        }

        if (it->second.game_over) {
            set_json(res, 400, json{{"error", "game is already over"}});
            return;
        }

        commander::GameState probe = it->second;
        if (in.contains("difficulty") && in["difficulty"].is_string()) {
            probe.difficulty = normalize_difficulty(in.value("difficulty", "medium"));
        }

        commander::Move m = commander::bot_move(probe);
        if (m.pid < 0) {
            set_json(res, 400, json{{"error", "hint could not find a legal move"}});
            return;
        }

        set_json(res, 200, json{{"move", move_to_json(m)}});
    });

    int port = 8080;
    if (const char* p = std::getenv("PORT")) {
        try { port = std::stoi(p); } catch (...) { port = 8080; }
    }

    std::printf("CommanderChess API listening on 0.0.0.0:%d\n", port);
    svr.listen("0.0.0.0", port);
    return 0;
}
