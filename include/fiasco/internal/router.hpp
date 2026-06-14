#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/routing/function_traits.hpp"
#include "fiasco/internal/routing/pathing.hpp"
#include "fiasco/internal/routing/route_table.hpp"

namespace fiasco::detail {

class router {
  public:
    router() = default;
    explicit router(std::string prefix)
        : m_prefix(std::move(prefix)) {}

    void print_routes() const {
        m_routes.for_each_route([](http_method, const std::string& path) {
            std::cout << path << "\n";
        });
        std::cout << "\n\n";
    }

    template <typename F>
    void get(const std::string& path, F&& f) {
        add(http_method::get, path, std::forward<F>(f));
    }

    template <typename F>
    void post(const std::string& path, F&& f) {
        add(http_method::post, path, std::forward<F>(f));
    }

    template <typename F>
    void put(const std::string& path, F&& f) {
        add(http_method::put, path, std::forward<F>(f));
    }

    template <typename F>
    void del(const std::string& path, F&& f) {
        add(http_method::del, path, std::forward<F>(f));
    }

    template <typename F>
    void patch(const std::string& path, F&& f) {
        add(http_method::patch, path, std::forward<F>(f));
    }

    void include_router(router sub, const std::string& prefix = "") {
        std::string full_prefix = join_prefix(prefix, sub.m_prefix);
        m_routes.merge_with_prefix(std::move(sub.m_routes), full_prefix);
    }

    match_result match(http_method method, const std::string& path) const {
        return m_routes.match(method, path);
    }

    bool any_method_matches(const std::string& path) const {
        return m_routes.any_method_matches(path);
    }

  private:
    route_table m_routes;
    const std::string m_prefix = "";

    template <typename F>
    void add(http_method method, const std::string& pattern, F&& f) {
        m_routes.add(method, pattern, make_handler(std::forward<F>(f)));
    }

    friend class server;
};

}  // namespace fiasco::detail