module;

#include "fiasco/internal/json.hpp"
#include "fiasco/internal/http/response.hpp"

export module fiasco.common;

export namespace fiasco {
    using json = detail::json;
    using response = detail::response;
}
