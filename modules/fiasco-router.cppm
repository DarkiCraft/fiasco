module;

#include "fiasco/internal/router.hpp"

export module fiasco.router;
export import fiasco.common;

export namespace fiasco {
    using router = detail::router;
}
