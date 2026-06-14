#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/routing/function_traits.hpp"
#include "fiasco/internal/routing/pathing.hpp"

namespace fiasco::detail {

constexpr size_t num_http_methods = 8;  // get, post, put, del, patch, head, options, unknown

inline size_t method_index(http_method m) {
    return static_cast<size_t>(m);
}

struct match_result {
    bool matched = false;
    handler_fn handler;
    std::vector<std::pair<std::string_view, std::string_view>> path_params;  // name -> value
};

class route_table {
  public:
    void add(http_method method, const std::string& pattern, handler_fn handler) {
        auto segs = split_path(pattern);

        bool has_param = false;
        for (auto seg : segs) {
            if (is_param_segment(seg)) {
                has_param = true;
                break;
            }
        }

        if (!has_param) {
            m_static[method_index(method)][pattern] = std::move(handler);
            return;
        }

        param_route_entry entry;
        entry.pattern = pattern;
        entry.segments.reserve(segs.size());
        entry.names.reserve(segs.size());
        entry.is_param_seg.reserve(segs.size());

        for (auto seg : segs) {
            if (is_param_segment(seg)) {
                entry.is_param_seg.push_back(true);
                entry.names.emplace_back(param_name(seg));
            } else {
                entry.is_param_seg.push_back(false);
                entry.names.emplace_back();
            }
            entry.segments.emplace_back(seg);
        }

        entry.handler = std::move(handler);
        m_param[method_index(method)].push_back(std::move(entry));
    }

    match_result match(http_method method, const std::string& path) const {
        auto& static_routes = m_static[method_index(method)];
        auto static_hit = static_routes.find(path);
        if (static_hit != static_routes.end()) {
            return {true, static_hit->second, {}};
        }

        auto& param_routes = m_param[method_index(method)];
        if (param_routes.empty()) {
            return {false};
        }

        auto req_segs = split_path(path);

        for (const auto& entry : param_routes) {
            if (entry.segments.size() != req_segs.size()) {
                continue;
            }

            bool ok = true;
            for (size_t i = 0; i < entry.segments.size(); ++i) {
                if (!entry.is_param_seg[i] && entry.segments[i] != req_segs[i]) {
                    ok = false;
                    break;
                }
            }

            if (!ok) {
                continue;
            }

            match_result result;
            result.matched = true;
            result.handler = entry.handler;
            result.path_params.reserve(entry.segments.size());
            for (size_t i = 0; i < entry.segments.size(); ++i) {
                if (entry.is_param_seg[i]) {
                    result.path_params.emplace_back(std::string_view{entry.names[i]}, req_segs[i]);
                }
            }
            return result;
        }

        return {false};
    }

    /// Used for 405 vs 404 disambiguation: does ANY method have a route
    /// matching this path?
    bool any_method_matches(const std::string& path) const {
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            if (m_static[mi].count(path)) {
                return true;
            }
        }

        auto req_segs = split_path(path);
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            for (const auto& entry : m_param[mi]) {
                if (entry.segments.size() != req_segs.size()) {
                    continue;
                }
                bool ok = true;
                for (size_t i = 0; i < entry.segments.size(); ++i) {
                    if (!entry.is_param_seg[i] && entry.segments[i] != req_segs[i]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    return true;
                }
            }
        }
        return false;
    }

    void merge_with_prefix(route_table&& other, std::string_view prefix) {
        for (size_t mi = 0; mi < num_http_methods; ++mi) {
            for (auto& [path, handler] : other.m_static[mi]) {
                std::string full = join_prefix(prefix, path);
                m_static[mi][std::move(full)] = std::move(handler);
            }
            other.m_static[mi].clear();

            for (auto& entry : other.m_param[mi]) {
                std::string full_pattern = join_prefix(prefix, entry.pattern);
                add(static_cast<http_method>(mi), full_pattern, std::move(entry.handler));
            }
            other.m_param[mi].clear();
        }
    }

    /// Iterates every registered route (method, full pattern) for
    /// diagnostics (e.g. print_routes).
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
        std::string pattern;                // full pattern, for diagnostics/include_router
        std::vector<std::string> segments;  // pre-split pattern segments (owned)
        std::vector<std::string> names;     // param name per segment ("" if static)
        std::vector<bool> is_param_seg;
        handler_fn handler;
    };

    std::array<std::unordered_map<std::string, handler_fn>, num_http_methods> m_static;
    std::array<std::vector<param_route_entry>, num_http_methods> m_param;
};

}  // namespace fiasco::detail