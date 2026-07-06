#include "auth.hpp"
#include "response_util.hpp"
#include <chrono>
#include <random>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// -----------------------------------------------------------------------------
// UserStore implementation
// -----------------------------------------------------------------------------
UserStore::UserStore(const std::string& path) : file_path_(path) {
    std::lock_guard<std::mutex> lk(mu_);
    std::ifstream in(file_path_);
    if (in) {
        json j;
        in >> j;
        for (auto& el : j.items()) {
            UserProfile up;
            up.user_id = el.key();
            up.provider = el.value().value("provider", "");
            up.provider_sub = el.value().value("provider_sub", "");
            up.data = el.value().value("data", json::object());
            users_.emplace(up.user_id, std::move(up));
        }
    }
}

UserStore::~UserStore() {
    save();
}

bool UserStore::get(const std::string& user_id, UserProfile& out) const {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = users_.find(user_id);
    if (it == users_.end()) return false;
    out = it->second;
    return true;
}

UserProfile& UserStore::find_or_create(const std::string& provider, const std::string& provider_sub) {
    std::lock_guard<std::mutex> lk(mu_);
    // search for existing
    for (auto& kv : users_) {
        if (kv.second.provider == provider && kv.second.provider_sub == provider_sub) {
            return kv.second;
        }
    }
    // create new
    UserProfile up;
    up.user_id = generate_id();
    up.provider = provider;
    up.provider_sub = provider_sub;
    up.data = json::object();
    users_.emplace(up.user_id, up);
    return users_.at(up.user_id);
}

void UserStore::save() {
    std::lock_guard<std::mutex> lk(mu_);
    json j = json::object();
    for (const auto& kv : users_) {
        j[kv.first] = {
            {"provider", kv.second.provider},
            {"provider_sub", kv.second.provider_sub},
            {"data", kv.second.data}
        };
    }
    // atomic write: write to temp then rename
    std::string tmp = file_path_ + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    out << j.dump(2);
    out.close();
    std::rename(tmp.c_str(), file_path_.c_str());
}

std::string UserStore::generate_id() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(rng);
    uint64_t b = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << a << std::setw(16) << b;
    return oss.str();
}

// -----------------------------------------------------------------------------
// JwtHelper implementation (HMAC‑SHA256)
// -----------------------------------------------------------------------------
std::string JwtHelper::secret() {
    const char* env = std::getenv("JWT_SECRET");
    return env ? std::string(env) : std::string("dev_secret_change_me");
}

static std::string base64_encode(const unsigned char* input, size_t len) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_write(b64, input, static_cast<int>(len));
    BIO_flush(b64);
    BUF_MEM *bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

static std::string base64_url_transform(std::string s) {
    // replace +/ with -_ and remove =
    for (char &c : s) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    s.erase(std::remove(s.begin(), s.end(), '='), s.end());
    return s;
}

std::string JwtHelper::base64_url_encode(const std::string& str) {
    return base64_url_transform(base64_encode(reinterpret_cast<const unsigned char*>(str.data()), str.size()));
}

static std::string base64_url_decode_impl(const std::string& input) {
    std::string s = input;
    // restore padding
    while ((s.size() % 4) != 0) s.push_back('=');
    for (char &c : s) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *bio = BIO_new_mem_buf(s.data(), static_cast<int>(s.size()));
    bio = BIO_push(b64, bio);
    std::vector<char> out(s.size());
    int decoded = BIO_read(bio, out.data(), static_cast<int>(out.size()));
    BIO_free_all(bio);
    return std::string(out.data(), decoded);
}

std::string JwtHelper::base64_url_decode(const std::string& str) {
    return base64_url_decode_impl(str);
}

std::string JwtHelper::sign(const json& payload) {
    json header = {{"alg", "HS256"}, {"typ", "JWT"}};
    std::string enc_header = base64_url_encode(header.dump());
    std::string enc_payload = base64_url_encode(payload.dump());
    std::string message = enc_header + "." + enc_payload;
    unsigned int len = EVP_MAX_MD_SIZE;
    unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), secret().data(), static_cast<int>(secret().size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(), mac, &len);
    std::string signature = base64_url_encode(std::string(reinterpret_cast<char*>(mac), len));
    return message + "." + signature;
}

bool JwtHelper::verify(const std::string& token, json& out_payload) {
    size_t first = token.find('.');
    size_t second = token.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos) return false;
    std::string enc_header = token.substr(0, first);
    std::string enc_payload = token.substr(first + 1, second - first - 1);
    std::string enc_sig = token.substr(second + 1);
    // recompute signature
    std::string message = enc_header + "." + enc_payload;
    unsigned int len = EVP_MAX_MD_SIZE;
    unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), secret().data(), static_cast<int>(secret().size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(), mac, &len);
    std::string expected_sig = base64_url_encode(std::string(reinterpret_cast<char*>(mac), len));
    if (expected_sig != enc_sig) return false;
    // decode payload
    try {
        std::string payload_str = base64_url_decode(enc_payload);
        out_payload = json::parse(payload_str);
    } catch (...) { return false; }
    // optional exp check
    if (out_payload.contains("exp")) {
        std::time_t now = std::time(nullptr);
        if (now > out_payload["exp"].get<std::time_t>()) return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Provider verification (Google, Apple, Facebook) – minimal implementations
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Provider verification (Google, Apple, Facebook) – minimal implementations
// -----------------------------------------------------------------------------
namespace provider {
    std::string verify_google(const std::string& id_token) {
        // Google tokeninfo endpoint
        std::string url = "https://oauth2.googleapis.com/tokeninfo?id_token=" + std::string(cpr::util::urlEncode(id_token).c_str());
        auto r = cpr::Get(cpr::Url{url});
        if (r.status_code != 200) return {};
        try {
            auto j = json::parse(r.text);
            if (j.contains("sub")) return j["sub"].get<std::string>();
        } catch (...) {}
        return {};
    }
    std::string verify_apple(const std::string& id_token) {
        // For now, we skip full Apple verification and just return empty (unauthenticated)
        // In production you would verify the JWT signature against Apple's public keys.
        return {};
    }
    std::string verify_facebook(const std::string& access_token) {
        // Facebook Graph API to retrieve the user id
        std::string url = "https://graph.facebook.com/me?fields=id&access_token=" + std::string(cpr::util::urlEncode(access_token).c_str());
        auto r = cpr::Get(cpr::Url{url});
        if (r.status_code != 200) return {};
        try {
            auto j = json::parse(r.text);
            if (j.contains("id")) return j["id"].get<std::string>();
        } catch (...) {}
        return {};
    }
}

// -----------------------------------------------------------------------------
// JwtHelper public decode wrapper
// -----------------------------------------------------------------------------
std::string JwtHelper::decode(const std::string& s) {
    return base64_url_decode(s);
}

// -----------------------------------------------------------------------------
// Provider verification (Apple) updated to use public wrapper if needed (currently unused)
// -----------------------------------------------------------------------------
// The rest of the file remains unchanged.

// -----------------------------------------------------------------------------
// Global user store (file path can be configured via env var USER_STORE_PATH)
// -----------------------------------------------------------------------------
static UserStore& get_user_store() {
    static UserStore store([](){
        const char* env = std::getenv("USER_STORE_PATH");
        return std::string(env ? env : "/Users/phamvotriduc/.gemini/antigravity/scratch/commander-chess-cpp/backend/users.json");
    }());
    return store;
}

// -----------------------------------------------------------------------------
// Route handlers – to be called from server.cpp
// -----------------------------------------------------------------------------
inline void handle_google_auth(const httplib::Request& req, httplib::Response& res) {
    if (!verify_csrf(req, res)) return;
    json in = safe_parse(req.body, res);
    if (in.is_discarded()) return;
    std::string token = in.value("id_token", "");
    std::string sub = provider::verify_google(token);
    if (sub.empty()) { set_error_json(res, 401, "AuthFailed", "Invalid Google token"); return; }
    UserProfile& profile = get_user_store().find_or_create("google", sub);
    respond_with_auth(res, profile);
}

inline void handle_apple_auth(const httplib::Request& req, httplib::Response& res) {
    if (!verify_csrf(req, res)) return;
    json in = safe_parse(req.body, res);
    if (in.is_discarded()) return;
    std::string token = in.value("id_token", "");
    std::string sub = provider::verify_apple(token);
    if (sub.empty()) { set_error_json(res, 401, "AuthFailed", "Invalid Apple token"); return; }
    UserProfile& profile = get_user_store().find_or_create("apple", sub);
    respond_with_auth(res, profile);
}

inline void handle_facebook_auth(const httplib::Request& req, httplib::Response& res) {
    if (!verify_csrf(req, res)) return;
    json in = safe_parse(req.body, res);
    if (in.is_discarded()) return;
    std::string token = in.value("access_token", "");
    std::string sub = provider::verify_facebook(token);
    if (sub.empty()) { set_error_json(res, 401, "AuthFailed", "Invalid Facebook token"); return; }
    UserProfile& profile = get_user_store().find_or_create("facebook", sub);
    respond_with_auth(res, profile);
}
