module;

#include "fiasco/internal/server.hpp"

export module fiasco.server;
export import fiasco.common;

export namespace fiasco {
    using server = detail::server;
}
