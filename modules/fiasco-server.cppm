module;

#include "fiasco/internal/server.hpp"

export module fiasco.server;
export import fiasco.common;
export import fiasco.response;

export namespace fiasco {
    using server = detail::server;
}
