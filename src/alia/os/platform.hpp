#ifndef ALIA_OS_PLATFORM_HPP
#define ALIA_OS_PLATFORM_HPP

#include "../events/event_source.hpp"

namespace alia {

// Shared source for process/platform events that have no originating window.
event_source &get_platform_event_source();

} // namespace alia

#endif // ALIA_OS_PLATFORM_HPP
