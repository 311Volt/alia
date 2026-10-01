#ifndef ALIA_HPP
#define ALIA_HPP

#include "core/vec.hpp"
#include "core/rect.hpp"
#include "core/color.hpp"
#include "core/get_time.hpp"
#include "core/timing.hpp"
#include "events/timer.hpp"
// #include "core/event_queue.hpp"
// #include "core/event_dispatcher.hpp"

#include "os/window.hpp"
#include "os/monitor.hpp"
#include "os/display.hpp"
#include "os/dialog.hpp"
#include "os/clipboard.hpp"
#include "os/platform.hpp"

#include "io/keycodes.hpp"
#include "io/keyboard.hpp"
#include "io/mouse.hpp"

namespace alia {

// Global initialization
void init();
void shutdown();

} // namespace alia

#endif // ALIA_HPP
