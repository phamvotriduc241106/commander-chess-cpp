#pragma once

#include <string>
#include <ctime>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <fstream>
#include "third_party/json.hpp"
#include "third_party/httplib.h"
#include "response_util.hpp"
#include <cpr/cpr.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

using json = nlohmann::json;

// Forward declarations for functions defined elsewhere
bool verify_csrf(const httplib::Request& req, httplib::Response& res);
json safe_parse(const std::string& body, httplib::Response& res);

// -----------------------------------------------------------------------------
// User profile definition (unchanged) ...
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// User profile definition
// -----------------------------------------------------------------------------
struct UserProfile {
    std::string user_id;          // internal UUID
    std::string provider;         // "google", "apple", "facebook"
    std::string provider_sub;     // provider's subject identifier
    json   data;                  // arbitrary game data (e.g., saved games)
};

// -----------------------------------------------------------------------------
// Simple in‑memory store backed by a JSON file
// -----------------------------------------------------------------------------
class UserStore {
public:
    UserStore(const std::string& path);
    ~UserStore();

    // Find by internal user_id
    bool get(const std::string& user_id, UserProfile& out) const;
    // Find or create by (provider, provider_sub)
    UserProfile& find_or_create(const std::string& provider, const std::string& provider_sub);
    // Persist to disk
    void save();

private:
    std::string file_path_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, UserProfile> users_; // key = user_id
    // helper to generate a random UUID‑like id
    static std::string generate_id();
};

// -----------------------------------------------------------------------------
// JWT handling (HMAC‑SHA256)
// -----------------------------------------------------------------------------
class JwtHelper {
public:
    // secret is read from env var JWT_SECRET or a default fallback (not for prod)
    static std::string sign(const json& payload);
    static bool verify(const std::string& token, json& out_payload);
    static std::string decode(const std::string& s);
private:
    static std::string secret();
    static std::string base64_url_encode(const std::string& str);
    static std::string base64_url_decode(const std::string& str);
};

// -----------------------------------------------------------------------------
// Provider token verification helpers (Google, Apple, Facebook)
// -----------------------------------------------------------------------------
namespace provider {
    // Returns provider subject (sub) on success, empty string on failure.
    std::string verify_google(const std::string& id_token);
    std::string verify_apple(const std::string& id_token);
    std::string verify_facebook(const std::string& id_token);
}

// -----------------------------------------------------------------------------
// Helper to add authentication JSON response (includes session token)
// -----------------------------------------------------------------------------
inline void set_auth_json(httplib::Response& res, int status, const json& body) {
    // reuse existing secure JSON helper for headers
    set_secure_json(res, status, body);
}

inline void handle_google_auth(const httplib::Request& req, httplib::Response& res);
inline void handle_apple_auth(const httplib::Request& req, httplib::Response& res);
inline void handle_facebook_auth(const httplib::Request& req, httplib::Response& res);

inline void respond_with_auth(httplib::Response& res, const UserProfile& profile) {
    // Create JWT payload with user info and expiration (24h)
    std::time_t now = std::time(nullptr);
    json payload = {
        {"user_id", profile.user_id},
        {"provider", profile.provider},
        {"sub", profile.provider_sub},
        {"iat", now},
        {"exp", now + 24 * 3600}
    };
    std::string token = JwtHelper::sign(payload);
    json body = {
        {"token", token},
        {"user", {
            {"user_id", profile.user_id},
            {"provider", profile.provider}
        }}
    };
    set_auth_json(res, 200, body);
}


