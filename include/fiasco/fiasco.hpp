#pragma once

#include "fiasco/internal/json.hpp"
#include "fiasco/internal/router.hpp"
#include "fiasco/internal/server.hpp"

namespace fiasco {

using json = detail::json;
using router = detail::router;
using server = detail::server;

}  // namespace fiasco