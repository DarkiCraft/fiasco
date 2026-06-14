#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace fiasco::detail {

/// Splits a path into segments, preserving emptiness information so that
/// trailing/leading/double slashes remain meaningful.
///
/// "/users/42"   -> ["users", "42"]
/// "/users/42/"  -> ["users", "42", ""]   (trailing slash => trailing empty segment)
/// "/"           -> [""]
/// "//a"         -> ["", "a"]
inline std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> segs;

    // Strip exactly one leading '/' if present
    if (!path.empty() && path.front() == '/') {
        path = path.substr(1);
    }

    if (path.empty()) {
        // "/" (after stripping the leading slash) -> single empty segment.
        segs.emplace_back("");
        return segs;
    }

    size_t start = 0;
    while (true) {
        size_t pos = path.find('/', start);
        if (pos == std::string_view::npos) {
            segs.push_back(path.substr(start));
            break;
        }
        segs.push_back(path.substr(start, pos - start));
        start = pos + 1;
    }

    return segs;
}

/// True if a segment is a "{name}" placeholder.
inline bool is_param_segment(std::string_view seg) {
    return seg.size() >= 3 && seg.front() == '{' && seg.back() == '}';
}

/// Extracts "name" from "{name}". Caller must ensure is_param_segment(seg).
inline std::string_view param_name(std::string_view seg) {
    return seg.substr(1, seg.size() - 2);
}

/// Joins a prefix and a path with exactly one '/' between them
/// join_prefix("/api/", "/users")  -> "/api/users"
/// join_prefix("/api", "/users")   -> "/api/users"
/// join_prefix("", "/users")       -> "/users"
/// join_prefix("/api", "/users/") -> "/api/users/"
/// join_prefix("/api/", "/users/") -> "/api/users/"
inline std::string join_prefix(std::string_view prefix, std::string_view path) {
    if (prefix.empty()) {
        return std::string(path);
    }

    bool prefix_ends_slash = prefix.back() == '/';
    bool path_starts_slash = !path.empty() && path.front() == '/';

    std::string result;
    result.reserve(prefix.size() + path.size() + 1);
    result += prefix;

    if (prefix_ends_slash && path_starts_slash) {
        result.append(path.substr(1));
    } else if (!prefix_ends_slash && !path_starts_slash && !path.empty()) {
        result += '/';
        result += path;
    } else {
        result += path;
    }

    return result;
}

}  // namespace fiasco::detail