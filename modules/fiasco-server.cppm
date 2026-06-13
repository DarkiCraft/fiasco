module;

#include "fiasco/internal/server.hpp"

export module fiasco.server;
export import fiasco.json;

export namespace fiasco {
    using server = detail::server;
}