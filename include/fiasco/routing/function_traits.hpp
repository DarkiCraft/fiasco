#ifndef FIASCO_ROUTING_FUNCTION_TRAITS_HPP
#define FIASCO_ROUTING_FUNCTION_TRAITS_HPP

/// @file function_traits.hpp
/// @brief Compile-time handler introspection and auto-dispatch.
///
/// Dispatch rules (applied left-to-right per parameter):
///   1. fiasco::request        -> raw request passed through
///   2. Primitive (int, double, float, bool, std::string)
///      -> next positional path param, converted from string
///   3. Non-primitive with from_json (FIASCO_MODEL)
///      -> deserialized from JSON request body (exactly one allowed)
///   4. Anything else          -> resolved from DI container
///
/// Return value rules:
///   - fiasco::response        -> passed through as-is
///   - T with to_json          -> serialized to JSON via response::json()
///
/// Primitives CANNOT be the body. Wrap in a struct.

#include <any>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "fiasco/http/request.hpp"
#include "fiasco/http/response.hpp"
#include "router.hpp"

namespace fiasco {
// -- DI container -------------------------------------------------------------

/// @brief Singleton-scoped DI container.
///
/// Factories are called once on first resolve; result is cached forever.
class di_container {
 public:
  /// @brief Registers a factory for type T. Called once; result is cached.
  template <typename T, typename Factory>
  void provide(Factory&& factory) {
    auto key = std::type_index(typeid(T));
    factories_[key] = [f = std::forward<Factory>(factory)]() -> std::any {
      return std::make_any<T>(f());
    };
  }

  /// @brief Resolves type T from the container. Returns a stable reference.
  /// @throws std::runtime_error if T was never registered.
  template <typename T>
  T& resolve() {
    auto key = std::type_index(typeid(T));

    auto sit = singletons_.find(key);
    if (sit != singletons_.end()) {
      return std::any_cast<T&>(sit->second);
    }

    auto fit = factories_.find(key);
    if (fit == factories_.end()) {
      throw std::runtime_error(
          std::string("DI: no provider registered for type: ") +
          typeid(T).name());
    }

    singletons_[key] = fit->second();
    return std::any_cast<T&>(singletons_[key]);
  }

  /// @brief Returns true if T has been registered.
  template <typename T>
  bool has() const {
    return factories_.count(std::type_index(typeid(T))) > 0;
  }

 private:
  std::unordered_map<std::type_index, std::function<std::any()>> factories_;
  std::unordered_map<std::type_index, std::any> singletons_;
};

// -- Primitive detection -----------------------------------------------------

template <typename T>
struct is_primitive : std::false_type {};

template <>
struct is_primitive<int> : std::true_type {};

template <>
struct is_primitive<long> : std::true_type {};

template <>
struct is_primitive<long long> : std::true_type {};

template <>
struct is_primitive<float> : std::true_type {};

template <>
struct is_primitive<double> : std::true_type {};

template <>
struct is_primitive<bool> : std::true_type {};

template <>
struct is_primitive<std::string> : std::true_type {};

template <typename T>
struct is_primitive<const T> : is_primitive<T> {};

template <typename T>
struct is_primitive<T&> : is_primitive<T> {};

template <typename T>
struct is_primitive<const T&> : is_primitive<T> {};

template <typename T>
constexpr bool is_primitive_v = is_primitive<T>::value;

// -- nlohmann ADL detection --------------------------------------------------

template <typename T, typename = void>
struct has_from_json : std::false_type {};

template <typename T>
struct has_from_json<
    T, std::void_t<decltype(from_json(std::declval<const nlohmann::json&>(),
                                      std::declval<T&>()))>> : std::true_type {
};

template <typename T>
constexpr bool has_from_json_v = has_from_json<T>::value;

template <typename T, typename = void>
struct has_to_json : std::false_type {};

template <typename T>
struct has_to_json<T,
                   std::void_t<decltype(to_json(std::declval<nlohmann::json&>(),
                                                std::declval<const T&>()))>>
    : std::true_type {};

template <typename T>
constexpr bool has_to_json_v = has_to_json<T>::value;

// -- Path param string -> T conversion -----------------------------------------

template <typename T>
T convert_path_param(const std::string& s) {
  if constexpr (std::is_same_v<T, std::string>) {
    return s;
  } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, long>) {
    return static_cast<T>(std::stol(s));
  } else if constexpr (std::is_same_v<T, long long>) {
    return std::stoll(s);
  } else if constexpr (std::is_same_v<T, float>) {
    return std::stof(s);
  } else if constexpr (std::is_same_v<T, double>) {
    return std::stod(s);
  } else if constexpr (std::is_same_v<T, bool>) {
    return s == "true" || s == "1";
  } else {
    static_assert(!sizeof(T),
                  "Unsupported primitive type for path param conversion");
  }
}

// -- Return value -> response serialization -----------------------------------

template <typename T>
response serialize_return(T&& val) {
  if constexpr (std::is_same_v<std::decay_t<T>, response>) {
    return std::forward<T>(val);
  } else if constexpr (has_to_json_v<std::decay_t<T>>) {
    nlohmann::json j = val;
    return response::to_json(j.dump());
  } else {
    static_assert(
        !sizeof(T),
        "Handler return type must be fiasco::response or a FIASCO_MODEL type");
  }
}

// -- Lambda traits -----------------------------------------------------------

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

// -- Per-argument resolver ---------------------------------------------------

/// @brief Resolves a single handler argument.
///
/// Dispatch order:
///   1. fiasco::request  -> pass raw request
///   2. primitive        -> next positional path param
///   3. has from_json    -> deserialize request body
///   4. anything else    -> resolve from DI container
template <typename T>
decltype(auto) resolve_arg(const request& req, size_t& path_idx,
                           di_container& di) {
  using CleanT = std::decay_t<T>;

  if constexpr (std::is_same_v<CleanT, request>) {
    return req;
  } else if constexpr (is_primitive_v<CleanT>) {
    if (path_idx >= req.ordered_path_params.size()) {
      throw std::runtime_error(
          "Not enough path params for handler argument at position " +
          std::to_string(path_idx));
    }
    return static_cast<CleanT>(
        convert_path_param<CleanT>(req.ordered_path_params[path_idx++]));
  } else if constexpr (has_from_json_v<CleanT>) {
    if (req.body.empty()) {
      throw std::runtime_error(
          "Handler expects a JSON body but request body is empty");
    }
    try {
      auto j = nlohmann::json::parse(req.body);
      CleanT val;
      from_json(j, val);
      return val;
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error(std::string("JSON body parse error: ") +
                               e.what());
    }
  } else {
    // DI — T& gets a stable reference, T gets a copy
    return static_cast<T>(di.resolve<CleanT>());
  }
}

// -- Dispatcher --------------------------------------------------------------

template <typename F, typename ArgsTuple, size_t... Is>
response dispatch_impl(F& fn, const request& req, size_t& path_idx,
                       di_container& di, std::index_sequence<Is...>) {
  if constexpr (std::is_void_v<std::invoke_result_t<F, std::tuple_element_t<Is, ArgsTuple>...>>) {
    fn(resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(req, path_idx, di)...);
    return response::to_empty();
  } else {
    return serialize_return(fn(
        resolve_arg<std::tuple_element_t<Is, ArgsTuple>>(req, path_idx, di)...));
  }
}

// -- make_handler ------------------------------------------------------------

// handler_fn is defined in router.hpp (already included above).

/// @brief Wraps any compatible callable into a normalized handler_fn.
///
/// The DI container is captured by reference — it must outlive all handlers.
template <typename F>
handler_fn make_handler(F&& f, di_container& di) {
  using traits = lambda_traits<std::decay_t<F>>;
  using args_tuple = typename traits::args_tuple;
  constexpr size_t arity = traits::arity;

  return [fn = std::forward<F>(f), &di](request req) mutable -> response {
    size_t path_idx = 0;
    return dispatch_impl<decltype(fn), args_tuple>(
        fn, req, path_idx, di, std::make_index_sequence<arity>{});
  };
}

}  // namespace fiasco

#endif  // FIASCO_ROUTING_FUNCTION_TRAITS_HPP
