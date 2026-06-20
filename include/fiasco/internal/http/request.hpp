#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fiasco::detail {

enum class http_method {
    get,
    post,
    put,
    del,  // "delete" is a C++ keyword
    patch,
    head,
    options,
    unknown
};

http_method string_to_method(const std::string& m);
const char* method_to_string(http_method m);

struct request {
    http_method method = http_method::unknown;
    std::string url;                // owns the URL buffer
    std::string_view path;          // points into url
    std::string_view query_string;  // points into url
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Named path params (views point into url for values, route table for keys)
    std::unordered_map<std::string_view, std::string_view> path_params;

    // Positional path param values in URL order (views point into url)
    std::vector<std::string_view> ordered_path_params;

    [[nodiscard]] std::string header(const std::string& key) const {
        auto it = headers.find(key);
        return (it != headers.end()) ? it->second : "";
    }

    [[nodiscard]] std::string content_type() const { return header("Content-Type"); }
};

}  // namespace fiasco::detail