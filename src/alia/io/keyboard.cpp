#include "keyboard.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <mutex>
#include <string_view>

namespace alia {
    namespace {
        constexpr key_mod lock_modifiers =
            key_mod::caps_lock | key_mod::num_lock | key_mod::scroll_lock;

        keyboard_state &tracked_keyboard_state() {
            static keyboard_state state;
            return state;
        }

        std::mutex &keyboard_state_mutex() {
            static std::mutex mutex;
            return mutex;
        }

        constexpr bool is_valid_key(key value) {
            const int index = static_cast<int>(value);
            return index > static_cast<int>(key::unknown) &&
                   index < static_cast<int>(key::key_count);
        }

        constexpr bool has_modifier(key_mod modifiers, key_mod modifier) {
            return (modifiers & modifier) != key_mod::none;
        }

        bool equal_key_names(std::string_view a, std::string_view b) {
            return a.size() == b.size() &&
                   std::equal(
                       a.begin(), a.end(), b.begin(),
                       [](char left, char right) {
                           return std::tolower(static_cast<unsigned char>(left)) ==
                                  std::tolower(static_cast<unsigned char>(right));
                       });
        }

        constexpr std::array key_names{
#define ALIA_KEYCODE(identifier, display_name) display_name,
#include "keycodes.inc"
#undef ALIA_KEYCODE
        };

        static_assert(
            key_names.size() == static_cast<std::size_t>(key::key_count));
    }

    keyboard_state::keyboard_state() = default;

    bool keyboard_state::is_key_down(key value) const {
        return is_valid_key(value) && keys_[static_cast<std::size_t>(value)];
    }

    key_mod keyboard_state::get_modifiers() const {
        return modifiers_;
    }

    bool keyboard_state::shift() const {
        return has_modifier(modifiers_, key_mod::shift);
    }

    bool keyboard_state::ctrl() const {
        return has_modifier(modifiers_, key_mod::ctrl);
    }

    bool keyboard_state::alt() const {
        return has_modifier(modifiers_, key_mod::alt);
    }

    bool keyboard_state::alt_gr() const {
        return has_modifier(modifiers_, key_mod::alt_gr);
    }

    bool keyboard_state::super() const {
        return has_modifier(modifiers_, key_mod::super);
    }

    bool keyboard_state::caps_lock() const {
        return has_modifier(modifiers_, key_mod::caps_lock);
    }

    bool keyboard_state::num_lock() const {
        return has_modifier(modifiers_, key_mod::num_lock);
    }

    bool keyboard_state::scroll_lock() const {
        return has_modifier(modifiers_, key_mod::scroll_lock);
    }

    void keyboard_state::refresh_modifiers(key_mod modifiers) {
        modifiers_ = modifiers & lock_modifiers;
        if (keys_[static_cast<std::size_t>(key::lshift)] ||
            keys_[static_cast<std::size_t>(key::rshift)])
            modifiers_ |= key_mod::shift;
        if (keys_[static_cast<std::size_t>(key::lctrl)] ||
            keys_[static_cast<std::size_t>(key::rctrl)])
            modifiers_ |= key_mod::ctrl;
        if (keys_[static_cast<std::size_t>(key::lalt)] ||
            keys_[static_cast<std::size_t>(key::ralt)])
            modifiers_ |= key_mod::alt;
        if (keys_[static_cast<std::size_t>(key::ralt)])
            modifiers_ |= key_mod::alt_gr;
        if (keys_[static_cast<std::size_t>(key::lsuper)] ||
            keys_[static_cast<std::size_t>(key::rsuper)])
            modifiers_ |= key_mod::super;
    }

    keyboard_state get_keyboard_state() {
        const std::lock_guard lock(keyboard_state_mutex());
        return tracked_keyboard_state();
    }

    bool is_key_down(key value) {
        const std::lock_guard lock(keyboard_state_mutex());
        return tracked_keyboard_state().is_key_down(value);
    }

    bool is_key_down(const keyboard_state &state, key value) {
        return state.is_key_down(value);
    }

    const char *get_key_name(key value) {
        const int index = static_cast<int>(value);
        if (index < 0 || index >= static_cast<int>(key::key_count))
            return key_names[0];
        return key_names[static_cast<std::size_t>(index)];
    }

    key get_key_from_name(const char *name) {
        if (!name)
            return key::unknown;
        const std::string_view requested(name);
        for (std::size_t i = 0; i < key_names.size(); ++i) {
            if (equal_key_names(requested, key_names[i]))
                return static_cast<key>(i);
        }
        return key::unknown;
    }

    namespace detail {
        void refresh_keyboard_modifiers(key_mod modifiers) {
            const std::lock_guard lock(keyboard_state_mutex());
            tracked_keyboard_state().refresh_modifiers(modifiers);
        }

        void update_keyboard_state(key value, bool down, key_mod modifiers) {
            const std::lock_guard lock(keyboard_state_mutex());
            auto &state = tracked_keyboard_state();
            if (is_valid_key(value))
                state.keys_[static_cast<std::size_t>(value)] = down;
            state.refresh_modifiers(modifiers);
        }

        void clear_keyboard_state(key_mod modifiers) {
            const std::lock_guard lock(keyboard_state_mutex());
            auto &state = tracked_keyboard_state();
            state.keys_.fill(false);
            state.modifiers_ = modifiers & lock_modifiers;
        }
    }
} // namespace alia
