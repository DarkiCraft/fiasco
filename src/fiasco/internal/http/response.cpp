#include "fiasco/internal/http/response.hpp"

#include <string>
#include <unordered_map>

#include "fiasco/internal/json.hpp"

namespace fiasco::detail {

response response::empty(int status) {
    response r;
    r.status_code = status;
    return r;
}

response response::text(const std::string& body, int status) {
    response r;
    r.status_code = status;
    r.body = body;
    r.body += '\n';
    r.headers["Content-Type"] = "text/plain";
    return r;
}

response response::json(const std::string& json_body, int status) {
    response r;
    r.status_code = status;
    r.body = json_body;
    r.body += '\n';
    r.headers["Content-Type"] = "application/json";
    return r;
}

response response::html(const std::string& body, int status) {
    response r;
    r.status_code = status;
    r.body = body;
    r.body += '\n';
    r.headers["Content-Type"] = "text/html";
    return r;
}

response response::error(const std::string& message, int status) {
    response r;
    r.status_code = status;
    ::fiasco::detail::json j;
    j["error"] = message;
    r.body = j.dump();
    r.body += '\n';
    r.headers["Content-Type"] = "application/json";
    return r;
}

[[nodiscard]] const char* response::reason_phrase(int code) {
    switch (code) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 422:
        return "Unprocessable Entity";
    case 500:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}

[[nodiscard]] std::string response::serialize() const {
    // pre-size to avoid reallocations: status line ~20, headers ~60 each, body
    std::string out;
    out.reserve(128 + headers.size() * 64 + body.size());

    // status line
    out += "HTTP/1.1 ";
    out += std::to_string(status_code);
    out += ' ';
    out += reason_phrase(status_code);
    out += "\r\n";

    bool has_content_length = false;
    bool has_connection = false;
    for (const auto& [key, val] : headers) {
        out += key;
        out += ": ";
        out += val;
        out += "\r\n";
        if (key == "Content-Length") {
            has_content_length = true;
        }
        if (key == "Connection") {
            has_connection = true;
        }
    }

    if (!has_content_length) {
        out += "Content-Length: ";
        out += std::to_string(body.size());
        out += "\r\n";
    }
    if (!has_connection) {
        out += "Connection: keep-alive\r\n";
    }

    out += "\r\n";
    out += body;
    return out;
}

}  // namespace fiasco::detail