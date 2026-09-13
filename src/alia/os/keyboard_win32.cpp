#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "../io/keyboard.hpp"

namespace alia {
    namespace {
        bool has_led(key_led leds, key_led led) {
            return (leds & led) != key_led::none;
        }

        void set_led(
            int virtual_key,
            bool extended,
            bool enabled,
            bool currently_enabled) {
            if (enabled == currently_enabled)
                return;
            const auto scan_code = static_cast<BYTE>(
                MapVirtualKeyW(static_cast<UINT>(virtual_key), MAPVK_VK_TO_VSC));
            const DWORD flags = extended ? KEYEVENTF_EXTENDEDKEY : 0;
            keybd_event(
                static_cast<BYTE>(virtual_key), scan_code, flags, 0);
            keybd_event(
                static_cast<BYTE>(virtual_key), scan_code,
                flags | KEYEVENTF_KEYUP, 0);
        }
    }

    key_led get_keyboard_leds() {
        BYTE state[256]{};
        if (!GetKeyboardState(state))
            return key_led::none;

        key_led leds = key_led::none;
        if ((state[VK_NUMLOCK] & 1) != 0)
            leds |= key_led::num_lock;
        if ((state[VK_CAPITAL] & 1) != 0)
            leds |= key_led::caps_lock;
        if ((state[VK_SCROLL] & 1) != 0)
            leds |= key_led::scroll_lock;
        return leds;
    }

    void set_keyboard_leds(key_led leds) {
        const key_led current = get_keyboard_leds();
        set_led(
            VK_NUMLOCK,
            true,
            has_led(leds, key_led::num_lock),
            has_led(current, key_led::num_lock));
        set_led(
            VK_CAPITAL,
            false,
            has_led(leds, key_led::caps_lock),
            has_led(current, key_led::caps_lock));
        set_led(
            VK_SCROLL,
            false,
            has_led(leds, key_led::scroll_lock),
            has_led(current, key_led::scroll_lock));
    }
} // namespace alia

#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
