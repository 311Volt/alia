#include "platform.hpp"

namespace alia {

event_source &get_platform_event_source() {
    static event_source source;
    return source;
}

} // namespace alia
