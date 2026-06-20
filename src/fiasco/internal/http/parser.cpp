#include "fiasco/internal/http/parser.hpp"

#include <llhttp.h>

#include <memory>
#include <string>

#include "fiasco/internal/http/request.hpp"

namespace fiasco::detail {

struct llhttp_parser::impl {
    struct parser_state {
        request req;
        std::string current_header_field;
        std::string current_header_value;
        bool last_was_value = false;
        bool headers_complete = false;
        bool message_complete = false;
        std::string url_buf;
    };

    llhttp_t m_parser{};
    llhttp_settings_t m_settings{};
    parser_state m_state;
    std::string m_error_reason;

    impl() {
        llhttp_settings_init(&m_settings);

        // -- Callbacks --
        m_settings.on_url = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->url_buf.append(at, len);
            return 0;
        };

        m_settings.on_header_field = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            if (s->last_was_value) {
                // Flush the previous header
                if (!s->current_header_field.empty()) {
                    s->req.headers[s->current_header_field] = s->current_header_value;
                }
                s->current_header_field.clear();
                s->current_header_value.clear();
                s->last_was_value = false;
            }
            s->current_header_field.append(at, len);
            return 0;
        };

        m_settings.on_header_value = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->current_header_value.append(at, len);
            s->last_was_value = true;
            return 0;
        };

        m_settings.on_headers_complete = [](llhttp_t* p) {
            auto* s = static_cast<parser_state*>(p->data);
            // Flush last header
            if (!s->current_header_field.empty()) {
                s->req.headers[s->current_header_field] = s->current_header_value;
                s->current_header_field.clear();
                s->current_header_value.clear();
            }

            // Set method
            s->req.method =
                string_to_method(llhttp_method_name(static_cast<llhttp_method_t>(p->method)));

            // Steal the allocation first
            s->req.url = std::move(s->url_buf);

            // Parse URL into path + query_string (views into req.url)
            auto qpos = s->req.url.find('?');
            if (qpos != std::string::npos) {
                s->req.path = std::string_view(s->req.url.data(), qpos);
                s->req.query_string =
                    std::string_view(s->req.url.data() + qpos + 1,
                                     s->req.url.size() - qpos - 1);
            } else {
                s->req.path = std::string_view(s->req.url);
                s->req.query_string = std::string_view();
            }

            s->headers_complete = true;
            return 0;
        };

        m_settings.on_body = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->req.body.append(at, len);
            return 0;
        };

        m_settings.on_message_complete = [](llhttp_t* p) {
            auto* s = static_cast<parser_state*>(p->data);
            s->message_complete = true;
            return 0;
        };
    }
};

llhttp_parser::llhttp_parser() : p_impl(std::make_unique<impl>()) {
    reset();
}

bool llhttp_parser::feed(const char* data, size_t len) {
    auto err = llhttp_execute(&p_impl->m_parser, data, len);
    if (err != HPE_OK && err != HPE_PAUSED) {
        p_impl->m_error_reason = llhttp_errno_name(err);
        return false;
    }
    return true;
}

[[nodiscard]] bool llhttp_parser::is_complete() const noexcept {
    return p_impl->m_state.message_complete;
}

[[nodiscard]] request llhttp_parser::take_request() {
    request r = std::move(p_impl->m_state.req);
    // Re-anchor path/query_string views into r.url (which now owns the buffer)
    if (!r.url.empty()) {
        auto qpos = r.url.find('?');
        if (qpos != std::string::npos) {
            r.path = std::string_view(r.url.data(), qpos);
            r.query_string =
                std::string_view(r.url.data() + qpos + 1,
                                 r.url.size() - qpos - 1);
        } else {
            r.path = std::string_view(r.url);
            r.query_string = std::string_view();
        }
    }
    reset();
    return r;
}

[[nodiscard]] const std::string& llhttp_parser::error() const noexcept {
    return p_impl->m_error_reason;
}

void llhttp_parser::reset() {
    p_impl->m_state = impl::parser_state{};
    llhttp_init(&p_impl->m_parser, HTTP_REQUEST, &p_impl->m_settings);
    p_impl->m_parser.data = &p_impl->m_state;
    p_impl->m_error_reason.clear();
}

llhttp_parser::~llhttp_parser() = default;

}  // namespace fiasco::detail