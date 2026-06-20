module;

#include "fiasco/internal/json.hpp"

export module fiasco.common;

export namespace fiasco {
    using json = detail::json;
}
