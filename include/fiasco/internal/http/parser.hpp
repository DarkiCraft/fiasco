#pragma once

#include <memory>
#include <string>

#include "fiasco/internal/http/request.hpp"

namespace fiasco::detail {

class llhttp_parser {
  public:
    llhttp_parser();

    bool feed(const char* data, size_t len);
    [[nodiscard]] bool is_complete() const noexcept;
    [[nodiscard]] request take_request();
    [[nodiscard]] const std::string& error() const noexcept;
    void reset();

    ~llhttp_parser();

  private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

}  // namespace fiasco::detail