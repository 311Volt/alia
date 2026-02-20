#ifndef ALIA_HPP
#define ALIA_HPP

#include "core/vec.hpp"
#include "core/rect.hpp"
#include "core/color.hpp"
// #include "core/event_queue.hpp"
// #include "core/event_dispatcher.hpp"

#include "os/window.hpp"
#include "os/monitor.hpp"
#include "os/display.hpp"
#include "os/dialog.hpp"
#include "os/clipboard.hpp"

#include "io/keycodes.hpp"
#include "io/keyboard.hpp"
#include "io/mouse.hpp"
#include "io/joystick.hpp"
#include "io/touch.hpp"

namespace alia {

// Global initialization
void init();
void shutdown();

// Time utilities
double get_time();

} // namespace alia

#endif // ALIA_HPP
