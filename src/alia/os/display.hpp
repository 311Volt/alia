#ifndef ALIA_OS_DISPLAY_HPP
#define ALIA_OS_DISPLAY_HPP

#include "../core/vec.hpp"
#include <vector>

namespace alia {

// Display mode (resolution, refresh rate, etc.)
struct display_mode {
    vec2i size;
    int refresh_rate;
    int color_depth;
    
    bool operator==(const display_mode& other) const = default;
};

// Get available display modes
[[nodiscard]] std::vector<display_mode> get_display_modes();
[[nodiscard]] std::vector<display_mode> get_display_modes(int monitor_index);

// Get current desktop mode
[[nodiscard]] display_mode get_desktop_mode();
[[nodiscard]] display_mode get_desktop_mode(int monitor_index);

// Find closest matching display mode
[[nodiscard]] display_mode find_closest_mode(vec2i size, int refresh_rate = 0);

} // namespace alia

#endif // ALIA_OS_DISPLAY_HPP
