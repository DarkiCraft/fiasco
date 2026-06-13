module;

#include "fiasco/internal/router.hpp"

export module fiasco.router;
export import fiasco.json;

export namespace fiasco {
    using router = detail::router;
}