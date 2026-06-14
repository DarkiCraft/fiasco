#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>

#include <nlohmann/json.hpp>

#include "fiasco/internal/http/request.hpp"
#include "fiasco/internal/http/response.hpp"
#include "fiasco/internal/routing/concepts.hpp"

namespace fiasco::detail {

using handler_fn = std::function<response(request)>;

template <Primitive T>
[[nodiscard]] T convert_path_param(const std::string& s) {
    if constexpr (std::same_as<T, std::string>) {
        return s;
    } else if constexpr (std::same_as<T, int> || std::same_as<T, long>) {
        return static_cast<T>(std::stol(s));
    } else if constexpr (std::same_as<T, long long>) {
        return std::stoll(s);
    } else if constexpr (std::same_as<T, float>) {
        return std::stof(s);
    } else if constexpr (std::same_as<T, double>) {
        return std::stod(s);
    } else if constexpr (std::same_as<T, bool>) {
        return s == "true" || s == "1";
    }

    return {};  // unreachable
}

template <typename T>
[[nodiscard]] response serialize_return(T&& val) {
    using CleanT = std::decay_t<T>;
    if constexpr (std::same_as<CleanT, response>) {
        return std::forward<T>(val);
    } else if constexpr (ToJson<CleanT>) {
        nlohmann::json j = val;
        return response::to_json(j.dump());
    } else {
        static_assert(!sizeof(T),
                      "Handler return type must be fiasco::response or a "
                      "FIASCO_MODEL type");
    }

    return {};  // unreachable
}

template <typename F>
struct lambda_traits : lambda_traits<decltype(&F::operator())> {};

template <typename C, typename R, typename... Args>
struct lambda_traits<R (C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename C, typename R, typename... Args>
struct lambda_traits<R (C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct lambda_traits<R (*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct lambda_traits<R (*)(Args...) noexcept> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

/// Dispatch order:
///   1. fiasco::request  -> pass raw request
///   2. Primitive        -> next positional path param
///   3. FromJson         -> deserialize request body
template <typename T>
decltype(auto) resolve_arg(const request& req, size_t& path_idx) {
    using CleanT = std::decay_t<T>;

    if constexpr (std::same_as<CleanT, request>) {
        return req;
    } else if constexpr (Primitive<CleanT>) {
        if (path_idx >= req.ordered_path_params.size()) {
            throw std::runtime_error("Not enough path params for handler argument at position " +
                                     std::to_string(path_idx));
        }
        return static_cast<CleanT>(convert_path_param<CleanT>(req.ordered_path_params[path_idx++]));
    } else if constexpr (FromJson<CleanT>) {
        if (req.body.empty()) {
            throw std::runtime_error("Handler expects a JSON body but request body is empty");
        }
        try {
            auto j = nlohmann::json::parse(req.body);
            CleanT val;
            from_json(j, val);
            return val;
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error(std::string("JSON body parse error: ") + e.what());
        }
    }
}

template <typename F, typename ArgsTuple, size_t... Is>
response dispatch_impl(F& fn, const request& req, size_t& path_idx, std::index_sequence<Is...>) {
    if constexpr (std::is_void_v<std::invoke_result_t<F, std::tuple_element_t<Is, ArgsTuple>...>>) {
        fn(resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(req, path_idx)...);
        return response::to_empty();
    } else {
        return serialize_return(
            fn(resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(req, path_idx)...));
    }
}

template <typename F>
[[nodiscard]] handler_fn make_handler(F&& f) {
    using traits = lambda_traits<std::decay_t<F>>;
    using args_tuple = traits::args_tuple;
    constexpr size_t arity = traits::arity;

    return [fn = std::forward<F>(f)](request req) mutable -> response {
        size_t path_idx = 0;
        return dispatch_impl<decltype(fn), args_tuple>(
            fn, req, path_idx, std::make_index_sequence<arity>{});
    };
}

}  // namespace fiasco::detail