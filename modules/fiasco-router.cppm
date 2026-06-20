module;

#include "fiasco/internal/router.hpp"

export module fiasco.router;
export import fiasco.common;
export import fiasco.response;

export namespace fiasco {
    using router = detail::router;
}
