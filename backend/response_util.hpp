#pragma once

#include <httplib.h>
#include "third_party/json.hpp"
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>

using json = nlohmann::json;

// Helper to split comma‑separated env var
inline std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // trim whitespace
        item.erase(0, item.find_first_not_of(" \t\r\n"));
        item.erase(item.find_last_not_of(" \t\r\n") + 1);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

// Retrieve allowed origins list (default "*")
inline std::vector<std::string> get_allowed_origins() {
    const char* env = std::getenv("ALLOWED_ORIGINS");
    if (!env) return {"*"};
    return split_csv(env);
}

// Set CORS header based on request Origin
inline void set_cors_header(httplib::Response& res, const std::string& request_origin) {
    auto origins = get_allowed_origins();
    if (std::find(origins.begin(), origins.end(), "*") != origins.end()) {
        res.set_header("Access-Control-Allow-Origin", "*");
    } else {
        for (const auto& o : origins) {
            if (o == request_origin) {
                res.set_header("Access-Control-Allow-Origin", o.c_str());
                break;
            }
        }
    }
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
}

// Set Content‑Security‑Policy header
inline void set_csp_header(httplib::Response& res) {
    const char* env = std::getenv("CSP_DIRECTIVE");
    std::string csp = env ? std::string(env) : "default-src 'none'; script-src 'none'; style-src 'none'; img-src 'none'; connect-src 'self'";
    res.set_header("Content-Security-Policy", csp.c_str());
}

// Secure JSON response helper – adds security related HTTP headers.
inline void set_secure_json(httplib::Response &res, int status, const json &body) {
    res.status = status;
    set_csp_header(res);
    // Security headers
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_content(body.dump(), "application/json");
}

// Standardised error JSON helper
inline void set_error_json(httplib::Response &res, int status, const std::string &type, const std::string &message) {
    json err = {
        {"code", status},
        {"type", type},
        {"message", message}
    };
    set_secure_json(res, status, err);
}

// Safe JSON parsing with size limit (8 KiB) – returns an empty json on error and writes an appropriate error response.
inline json safe_parse(const std::string &body, httplib::Response &res) {
    constexpr size_t MAX_BODY = 8 * 1024; // 8 KiB
    if (body.size() > MAX_BODY) {
        set_error_json(res, 413, "PayloadTooLarge", "payload too large - max 8 KiB");
        return {};
    }
    try {
        return json::parse(body);
    } catch (const std::exception &) {
        set_error_json(res, 400, "InvalidJSON", "invalid JSON payload");
        return {};
    }
}

// Simple in‑memory IP rate limiter (requests per minute). Rate limit can be set via env var RATE_LIMIT, default 100.
struct RateInfo {
    int count = 0;
    std::chrono::steady_clock::time_point window_start;
};

static std::unordered_map<std::string, RateInfo> g_rate_map;
static std::mutex g_rate_mu;
static const int DEFAULT_RATE_LIMIT = 100; // deprecated, use RATE_LIMIT env
inline int get_rate_limit() {
    const char* env = std::getenv("RATE_LIMIT");
    return env ? std::stoi(env) : DEFAULT_RATE_LIMIT;
}
static const std::chrono::seconds WINDOW = std::chrono::seconds(60);

inline bool check_rate_limit(const std::string &ip, httplib::Response &res) {
    std::lock_guard<std::mutex> lk(g_rate_mu);
    auto &info = g_rate_map[ip];
    auto now = std::chrono::steady_clock::now();
    const int LIMIT = get_rate_limit();
    if (now - info.window_start > WINDOW) {
        info.window_start = now;
        info.count = 0;
    }
    ++info.count;
    if (info.count > LIMIT) {
        set_error_json(res, 429, "RateLimitExceeded", "rate limit exceeded");
        return false;
    }
    res.set_header("X-RateLimit-Limit", std::to_string(LIMIT));
    res.set_header("X-RateLimit-Remaining", std::to_string(LIMIT - info.count));
    res.set_header("X-RateLimit-Reset", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(info.window_start + WINDOW - now).count()));
    return true;
}

// Set authentication/session cookie with secure flags
inline void set_secure_cookie(httplib::Response &res, const std::string &name, const std::string &value, int max_age_seconds = 0) {
    std::string cookie = name + "=" + value + "; Path=/; Secure; HttpOnly; SameSite=Strict";
    if (max_age_seconds > 0) {
        cookie += "; Max-Age=" + std::to_string(max_age_seconds);
    }
    res.set_header("Set-Cookie", cookie.c_str());
}

// Session TTL helper – env var SESSION_TTL_HOURS, default 24 hours.
inline std::chrono::hours session_ttl() {
    const char* env = std::getenv("SESSION_TTL_HOURS");
    int hrs = env ? std::stoi(env) : 24;
    return std::chrono::hours(hrs);
}
