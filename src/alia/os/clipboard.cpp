#include "clipboard.hpp"
#include "os_context.hpp"

namespace alia::clipboard {
    namespace {
        void validate(os_context &c) {
            if (!c)
                throw std::invalid_argument("clipboard requires a valid OS context");
        }
    } // namespace

    bool has_text(os_context &c) {
        validate(c);
        return c.backend()->clipboard_has_text(c.handle());
    }

    std::string get_text(os_context &c) {
        validate(c);
        return c.backend()->clipboard_get_text(c.handle());
    }

    bool set_text(os_context &c, std::string_view text) {
        validate(c);
        return c.backend()->clipboard_set_text(c.handle(), text);
    }

    bool has_image(os_context &c) {
        validate(c);
        return c.backend()->clipboard_has_image(c.handle());
    }

    void clear(os_context &c) {
        validate(c);
        c.backend()->clipboard_clear(c.handle());
    }

    bool has_text() {
        return has_text(get_os_context());
    }

    std::string get_text() {
        return get_text(get_os_context());
    }

    bool set_text(std::string_view text) {
        return set_text(get_os_context(), text);
    }

    bool has_image() {
        return has_image(get_os_context());
    }

    void clear() {
        clear(get_os_context());
    }
} // namespace alia::clipboard
