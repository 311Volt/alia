#include "x11_ops.hpp"
#ifdef ALIA_COMPILE_PLATFORM_BACKEND_X11
#include <mutex>

namespace alia {
    namespace {
        thread_local x11_error_scope *current_scope = nullptr;
        XErrorHandler previous_handler = nullptr;

        int handle_error(Display *display, XErrorEvent *event) {
            for (auto *scope = current_scope; scope; scope = scope->previous) {
                if (scope->display == display && event->serial >= scope->first_request) {
                    scope->error = event->error_code;
                    return 0;
                }
            }
            return previous_handler ? previous_handler(display, event) : 0;
        }
    } // namespace

    void x11_install_error_handler() {
        static std::once_flag once;
        std::call_once(once, [] { previous_handler = XSetErrorHandler(handle_error); });
    }

    x11_error_scope::x11_error_scope(Display *d) : display(d), first_request(0), previous(current_scope) {
        XSync(display, False);
        first_request = NextRequest(display);
        current_scope = this;
    }

    int x11_error_scope::sync() {
        XSync(display, False);
        return error;
    }

    x11_error_scope::~x11_error_scope() {
        sync();
        current_scope = previous;
    }
} // namespace alia
#endif
