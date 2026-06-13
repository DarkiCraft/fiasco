#pragma once

#include <string>
#include <unordered_map>

#include "fiasco/internal/json.hpp"

namespace fiasco::detail {

struct response {
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    static response to_empty(int status = 204) {
        response r;
        r.status_code = status;
        return r;
    }

    static response to_text(const std::string& body, int status = 200) {
        response r;
        r.status_code = status;
        r.body = body + '\n';
        r.headers["Content-Type"] = "text/plain";
        return r;
    }

    static response to_json(const std::string& json_body, int status = 200) {
        response r;
        r.status_code = status;
        r.body = json_body + '\n';
        r.headers["Content-Type"] = "application/json";
        return r;
    }

    static response to_error(const std::string& message, int status) {
        response r;
        r.status_code = status;
        json_type j;
        j["error"] = message;
        r.body = j.dump() + '\n';
        r.headers["Content-Type"] = "application/json";
        return r;
    }

    [[nodiscard]] static const char* reason_phrase(int code) {
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

    [[nodiscard]] std::string serialize() const {
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
            if (key == "Content-Length") {has_content_length = true;}
            if (key == "Connection")     {has_connection = true;}
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
};

}  // namespace fiasco::detail