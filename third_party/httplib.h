#pragma once
// Minimal cpp-httplib placeholder header for compilation.
// In production, replace with the official cpp-httplib library (https://github.com/yhirose/cpp-httplib).
#include <string>
#include <functional>
namespace httplib {
    struct Request {
        std::string method;
        std::string path;
        std::string body;
        std::string get_header_value(const std::string&) const { return {}; }
        std::string remote_addr;
    };
    struct Response {
        int status = 200;
        void set_header(const std::string&, const std::string&) {}
        void set_content(const std::string&, const std::string&) {}
    };
    class Server {
    public:
        using Handler = std::function<void(const Request&, Response&)>;
        void Post(const std::string&, Handler) {}
        void Get(const std::string&, Handler) {}
        void Options(const std::string&, Handler) {}
        void set_pre_routing_handler(std::function<int(const Request&, Response&)>) {}
        void listen(const char*, int) {}
    };
}
