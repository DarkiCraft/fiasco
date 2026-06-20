#pragma once

#include <string>
#include <utility>

#include "fiasco/internal/core/tcp_server.hpp"
#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/json.hpp"
#include "fiasco/internal/router.hpp"

namespace fiasco::detail {

class server {
  public:
    void print_routes() const { m_router.print_routes(); }

    explicit server(unsigned int num_threads = 0)
        : m_threads(num_threads) {}

    template <typename F>
    void get(const std::string& path, F&& f) {
        m_router.add(http_method::get, path, std::forward<F>(f));
    }

    template <typename F>
    void post(const std::string& path, F&& f) {
        m_router.add(http_method::post, path, std::forward<F>(f));
    }

    template <typename F>
    void put(const std::string& path, F&& f) {
        m_router.add(http_method::put, path, std::forward<F>(f));
    }

    template <typename F>
    void del(const std::string& path, F&& f) {
        m_router.add(http_method::del, path, std::forward<F>(f));
    }

    template <typename F>
    void patch(const std::string& path, F&& f) {
        m_router.add(http_method::patch, path, std::forward<F>(f));
    }

    void include_router(router sub, const std::string& prefix = "") {
        m_router.include_router(std::move(sub), prefix);
    }

    void run(uint16_t port = 8080, const std::string& host = "0.0.0.0") {
        tcp_server server(
            port, host, [this](request req) { return dispatch(std::move(req)); }, m_threads);
        server.run();
    }

    void stop() {}

  private:
    router m_router;
    unsigned int m_threads = 0;

    response dispatch(request req) {
        auto match = m_router.match(req.method, req.path);

        if (!match.matched) {
            if (m_router.any_method_matches(req.path)) {
                return response::error("Method Not Allowed", 405);
            }
            return response::error("Not Found", 404);
        }

        req.path_params.clear();
        req.ordered_path_params.clear();
        for (const auto& [name, value] : match.path_params) {
            req.path_params.emplace(name, value);
            req.ordered_path_params.push_back(value);
        }

        try {
            return match.handler(std::move(req));
        } catch (const json::exception& e) {
            return response::error(e.what(), 422);
        } catch (const std::exception& e) {
            return response::error(e.what(), 500);
        } catch (...) {
            return response::error("Internal Server Error", 500);
        }
    }
};

}  // namespace fiasco::detail