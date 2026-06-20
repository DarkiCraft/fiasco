#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/routing/function_traits.hpp"

namespace fiasco::detail {

constexpr size_t num_http_methods = 8;

inline size_t method_index(http_method m) {
    return static_cast<size_t>(m);
}

struct string_hash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string>{}(s);
    }
};

struct match_result {
    bool matched = false;
    handler_fn handler;
    std::vector<std::pair<std::string_view, std::string_view>> path_params;
};

class route_table {
  public:
    void add(http_method method, const std::string& pattern, handler_fn handler);
    match_result match(http_method method, std::string_view path) const;
    bool any_method_matches(std::string_view path) const;
    void merge_with_prefix(route_table&& other, std::string_view prefix);

    template <typename F>
    void for_each_route(F&& f) const {
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            for (const auto& [path, handler] : m_static[mi]) {
                f(static_cast<http_method>(mi), path);
            }
            for (const auto& entry : m_param[mi]) {
                f(static_cast<http_method>(mi), entry.pattern);
            }
        }
    }

  private:
    struct param_route_entry {
        std::string pattern;
        std::vector<std::string> segments;
        std::vector<std::string> names;
        std::vector<bool> is_param_seg;
        handler_fn handler;
    };

    std::array<std::unordered_map<std::string, handler_fn, string_hash, std::equal_to<>>, num_http_methods> m_static;
    std::array<std::vector<param_route_entry>, num_http_methods> m_param;
};

}  // namespace fiasco::detail