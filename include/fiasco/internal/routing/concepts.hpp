#pragma once

#include <concepts>
#include <string>

#include "fiasco/internal/json.hpp"

namespace fiasco::detail {

template <typename T>
concept Primitive = std::same_as<T, int> || std::same_as<T, long> || std::same_as<T, long long> ||
                    std::same_as<T, float> || std::same_as<T, double> || std::same_as<T, bool> ||
                    std::same_as<T, std::string>;

template <typename T>
concept FromJson = requires(const json& j, T& v) { from_json(j, v); };

template <typename T>
concept ToJson = requires(json& j, const T& v) { to_json(j, v); };

}  // namespace fiasco::detail