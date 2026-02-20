#ifndef ALIA_IO_TOUCH_HPP
#define ALIA_IO_TOUCH_HPP

#include "../core/vec.hpp"
#include "../core/event_queue.hpp"
#include <vector>

namespace alia {

// Touch point
struct touch_point {
    int id;              // Unique identifier for this touch
    vec2f position;
    vec2f pressure;      // 0.0 to 1.0, if available
    bool is_primary;     // First touch in a multi-touch sequence
};

// Touch state
class touch_state {
public:
    touch_state();
    
    [[nodiscard]] int touch_count() const;
    [[nodiscard]] const touch_point& get_touch(int index) const;
    [[nodiscard]] const touch_point* find_touch(int id) const;
    
    // Iterate touches
    [[nodiscard]] auto begin() const { return touches_.begin(); }
    [[nodiscard]] auto end() const { return touches_.end(); }
    
private:
    std::vector<touch_point> touches_;
};

// Get current touch state
[[nodiscard]] touch_state get_touch_state();

// Check touch support
[[nodiscard]] bool has_touch_input();
[[nodiscard]] int get_max_touches();

// Get event source for touch events
event_source& get_touch_event_source();

} // namespace alia

#endif // ALIA_IO_TOUCH_HPP
