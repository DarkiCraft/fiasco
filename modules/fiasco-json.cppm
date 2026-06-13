module;

#include "fiasco/internal/json.hpp"

export module fiasco.json;

export namespace fiasco {
    using json = detail::json_type;
}