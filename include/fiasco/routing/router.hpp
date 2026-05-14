#ifndef FIASCO_ROUTING_ROUTER_HPP
#define FIASCO_ROUTING_ROUTER_HPP

/// @file router.hpp
/// @brief Route registration and matching for fiasco.
///
/// Supports static routes ("/users") and single-segment path parameters
/// ("/users/{id}"). Routes are stored per HTTP method, split at registration
/// into a static map (O(1) lookup) and a parameterized list (linear scan,
/// only reached on static miss). Request paths are split once per match call.

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "fiasco/http/request.hpp"
#include "fiasco/http/response.hpp"

namespace fiasco {

using handler_fn = std::function<response(request)>;

/// @brief Splits a path string into segments by '/'.
/// "/users/42" -> ["users", "42"]
inline std::vector<std::string> split_path(const std::string& path) {
  std::vector<std::string> segs;
  std::string cur;
  cur.reserve(32);
  for (char c : path) {
    if (c == '/') {
      if (!cur.empty()) {
        segs.push_back(std::move(cur));
        cur.clear();
      }
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) {segs.push_back(std::move(cur));}
  return segs;
}

/// @brief Returns true if segment is a {param} placeholder.
inline bool is_param(const std::string& seg) {
  return seg.size() >= 3 && seg.front() == '{' && seg.back() == '}';
}

/// @brief Extracts the param name from "{id}" -> "id".
inline std::string param_name(const std::string& seg) {
  return seg.substr(1, seg.size() - 2);
}

/// @brief Result of a route match.
struct match_result {
  bool matched = false;
  handler_fn handler;
  std::unordered_map<std::string, std::string> path_params;   // by name
  std::vector<std::string> ordered_path_params;               // by position
};

/// @brief A registered parameterized route (has at least one {param} segment).
struct param_route_entry {
  std::vector<std::string> segments;   // pre-split pattern segments
  std::vector<std::string> names;      // param name per segment ("" if static)
  std::vector<bool> is_param_seg;      // pre-computed per-segment flag
  handler_fn handler;
};

/// @brief The router. Stores routes per HTTP method.
///
/// Static routes live in a flat unordered_map for O(1) lookup.
/// Parameterized routes are in a vector, only scanned on static miss.
class router {
 public:
  /// @brief Registers a route for the given method and pattern.
  void add_route(http_method method, const std::string& pattern,
                 handler_fn handler) {
    auto segs = split_path(pattern);

    // Check if any segment is a param placeholder.
    bool has_param = false;
    for (const auto& s : segs)
      {if (is_param(s)) { has_param = true; break; }}

    if (!has_param) {
      // Static route: direct map insertion, O(1) match.
      m_static_routes[method][pattern] = std::move(handler);
    } else {
      // Parameterized route: precompute per-segment metadata once.
      param_route_entry entry;
      entry.segments = segs;
      entry.is_param_seg.reserve(segs.size());
      entry.names.reserve(segs.size());
      for (const auto& s : segs) {
        if (is_param(s)) {
          entry.is_param_seg.push_back(true);
          entry.names.push_back(param_name(s));
        } else {
          entry.is_param_seg.push_back(false);
          entry.names.emplace_back();
        }
      }
      entry.handler = std::move(handler);
      m_param_routes[method].push_back(std::move(entry));
    }
  }

  /// @brief Attempts to match a request path against registered routes.
  ///
  /// Tries the static map first (O(1)). On miss, scans parameterized routes
  /// linearly. Request path is split once per call.
  match_result match(http_method method, const std::string& path) const {
    // --- Static lookup (O(1)) ---
    {
      auto mit = m_static_routes.find(method);
      if (mit != m_static_routes.end()) {
        auto hit = mit->second.find(path);
        if (hit != mit->second.end())
          {return {true, hit->second, {}, {}};}
      }
    }

    // --- Parameterized scan ---
    auto pit = m_param_routes.find(method);
    if (pit == m_param_routes.end()){ return {false};}

    const auto req_segs = split_path(path);

    for (const auto& entry : pit->second) {
      if (entry.segments.size() != req_segs.size()) {continue;}

      std::unordered_map<std::string, std::string> params;
      std::vector<std::string> ordered;
      bool ok = true;

      for (size_t i = 0; i < entry.segments.size(); ++i) {
        if (entry.is_param_seg[i]) {
          params[entry.names[i]] = req_segs[i];
          ordered.push_back(req_segs[i]);
        } else if (entry.segments[i] != req_segs[i]) {
          ok = false;
          break;
        }
      }

      if (ok)
        {return {true, entry.handler, std::move(params), std::move(ordered)};}
    }

    return {false};
  }

  /// @brief Returns true if the path matches ANY method (used for 405).
  bool any_method_matches(const std::string& path) const {
    // Check static routes first across all methods.
    for (const auto& [method, map] : m_static_routes)
      {if (map.count(path)) return true;}

    // Then parameterized — split once, reuse.
    const auto req_segs = split_path(path);
    for (const auto& [method, entries] : m_param_routes) {
      for (const auto& entry : entries) {
        if (entry.segments.size() != req_segs.size()) {continue;}
        bool ok = true;
        for (size_t i = 0; i < entry.segments.size(); ++i) {
          if (!entry.is_param_seg[i] && entry.segments[i] != req_segs[i]) {
            ok = false;
            break;
          }
        }
        if (ok){ return true;}
      }
    }
    return false;
  }

 private:
  // Static routes: method -> exact path -> handler
  std::unordered_map<http_method,
      std::unordered_map<std::string, handler_fn>> m_static_routes;

  // Parameterized routes: method -> list of entries. Linear scan on miss.
  std::unordered_map<http_method,
      std::vector<param_route_entry>> m_param_routes;
};

}  // namespace fiasco

#endif  // FIASCO_ROUTING_ROUTER_HPP