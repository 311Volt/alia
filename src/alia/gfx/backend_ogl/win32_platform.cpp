#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "ogl_platform.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

namespace alia {
    namespace {
        constexpr int WGL_DRAW_TO_WINDOW_ARB_ = 0x2001;
        constexpr int WGL_SWAP_METHOD_ARB_ = 0x2007;
        constexpr int WGL_SUPPORT_OPENGL_ARB_ = 0x2010;
        constexpr int WGL_DOUBLE_BUFFER_ARB_ = 0x2011;
        constexpr int WGL_PIXEL_TYPE_ARB_ = 0x2013;
        constexpr int WGL_COLOR_BITS_ARB_ = 0x2014;
        constexpr int WGL_RED_BITS_ARB_ = 0x2015;
        constexpr int WGL_RED_SHIFT_ARB_ = 0x2016;
        constexpr int WGL_GREEN_BITS_ARB_ = 0x2017;
        constexpr int WGL_GREEN_SHIFT_ARB_ = 0x2018;
        constexpr int WGL_BLUE_BITS_ARB_ = 0x2019;
        constexpr int WGL_BLUE_SHIFT_ARB_ = 0x201A;
        constexpr int WGL_ALPHA_BITS_ARB_ = 0x201B;
        constexpr int WGL_ALPHA_SHIFT_ARB_ = 0x201C;
        constexpr int WGL_ACCUM_RED_BITS_ARB_ = 0x201E;
        constexpr int WGL_ACCUM_GREEN_BITS_ARB_ = 0x201F;
        constexpr int WGL_ACCUM_BLUE_BITS_ARB_ = 0x2020;
        constexpr int WGL_ACCUM_ALPHA_BITS_ARB_ = 0x2021;
        constexpr int WGL_DEPTH_BITS_ARB_ = 0x2022;
        constexpr int WGL_STENCIL_BITS_ARB_ = 0x2023;
        constexpr int WGL_AUX_BUFFERS_ARB_ = 0x2024;
        constexpr int WGL_SWAP_EXCHANGE_ARB_ = 0x2028;
        constexpr int WGL_SWAP_COPY_ARB_ = 0x2029;
        constexpr int WGL_SWAP_UNDEFINED_ARB_ = 0x202A;
        constexpr int WGL_TYPE_RGBA_ARB_ = 0x202B;
        constexpr int WGL_SAMPLE_BUFFERS_ARB_ = 0x2041;
        constexpr int WGL_SAMPLES_ARB_ = 0x2042;
        constexpr int WGL_TYPE_RGBA_FLOAT_ARB_ = 0x21A0;

        using choose_pixel_format_arb_fn = BOOL (WINAPI *)(
            HDC, const int *, const FLOAT *, UINT, int *, UINT *);
        using get_pixel_format_attrib_arb_fn = BOOL (WINAPI *)(
            HDC, int, int, UINT, const int *, int *);
        using create_context_attribs_arb_fn = HGLRC (WINAPI *)(HDC, HGLRC, const int *);
        using swap_interval_ext_fn = BOOL (WINAPI *)(int);
        using get_swap_interval_ext_fn = int (WINAPI *)();

        template <class T>
        T load_wgl_proc(const char *name) {
            PROC proc = wglGetProcAddress(name);
            const std::intptr_t value = reinterpret_cast<std::intptr_t>(proc);
            if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1)
                return nullptr;
            return reinterpret_cast<T>(proc);
        }

        HWND create_ogl_dummy_hwnd() {
            static const wchar_t *cls = L"AliaDummy_OGL";
            static bool registered = false;
            if (!registered) {
                WNDCLASSEXW wc = {};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = DefWindowProcW;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = cls;
                RegisterClassExW(&wc);
                registered = true;
            }
            return CreateWindowExW(
                0, cls, L"", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        }

        bool set_default_pixel_format(HDC hdc) {
            PIXELFORMATDESCRIPTOR pfd = {};
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = 32;
            pfd.cDepthBits = 24;
            pfd.cStencilBits = 8;
            pfd.iLayerType = PFD_MAIN_PLANE;
            const int format = ChoosePixelFormat(hdc, &pfd);
            return format && SetPixelFormat(hdc, format, &pfd) == TRUE;
        }

        struct win_ogl_ctx {
            HWND dummy_hwnd = nullptr;
            HDC dummy_hdc = nullptr;
            HGLRC hglrc = nullptr;
        };

        struct win_ogl_surface {
            HWND hwnd = nullptr;
            HDC hdc = nullptr;
            HGLRC hglrc = nullptr;
            int pixel_format = 0;
            framebuffer_properties props;
        };

        bool any_framebuffer_option_requested(const framebuffer_config &c) {
            return c.color_bits.requested() || c.red_bits.requested() ||
                c.green_bits.requested() || c.blue_bits.requested() ||
                c.alpha_bits.requested() || c.red_shift.requested() ||
                c.green_shift.requested() || c.blue_shift.requested() ||
                c.alpha_shift.requested() || c.depth_bits.requested() ||
                c.stencil_bits.requested() || c.samples.requested() ||
                c.aux_buffers.requested() || c.accum_red_bits.requested() ||
                c.accum_green_bits.requested() || c.accum_blue_bits.requested() ||
                c.accum_alpha_bits.requested() || c.float_color.requested() ||
                c.float_depth.requested() || c.single_buffer.requested() ||
                c.update_display_region.requested() || c.swap.requested();
        }

        template <class T>
        void append_option(
            std::vector<int> &attributes,
            int attribute,
            const gfx_option<T> &option,
            bool include_suggested) {
            if (!option.requested() || (!include_suggested && !option.required()))
                return;
            attributes.push_back(attribute);
            attributes.push_back(static_cast<int>(option.value));
        }

        std::vector<int> pixel_format_attributes(
            const framebuffer_config &config,
            bool include_suggested) {
            std::vector<int> result{
                WGL_DRAW_TO_WINDOW_ARB_, TRUE,
                WGL_SUPPORT_OPENGL_ARB_, TRUE,
            };
            const auto initial_size = result.size();
            append_option(result, WGL_COLOR_BITS_ARB_, config.color_bits, include_suggested);
            append_option(result, WGL_RED_BITS_ARB_, config.red_bits, include_suggested);
            append_option(result, WGL_GREEN_BITS_ARB_, config.green_bits, include_suggested);
            append_option(result, WGL_BLUE_BITS_ARB_, config.blue_bits, include_suggested);
            append_option(result, WGL_ALPHA_BITS_ARB_, config.alpha_bits, include_suggested);
            append_option(result, WGL_DEPTH_BITS_ARB_, config.depth_bits, include_suggested);
            append_option(result, WGL_STENCIL_BITS_ARB_, config.stencil_bits, include_suggested);
            append_option(result, WGL_AUX_BUFFERS_ARB_, config.aux_buffers, include_suggested);
            append_option(result, WGL_ACCUM_RED_BITS_ARB_, config.accum_red_bits, include_suggested);
            append_option(result, WGL_ACCUM_GREEN_BITS_ARB_, config.accum_green_bits, include_suggested);
            append_option(result, WGL_ACCUM_BLUE_BITS_ARB_, config.accum_blue_bits, include_suggested);
            append_option(result, WGL_ACCUM_ALPHA_BITS_ARB_, config.accum_alpha_bits, include_suggested);

            if (config.samples.requested() &&
                (include_suggested || config.samples.required())) {
                result.push_back(WGL_SAMPLE_BUFFERS_ARB_);
                result.push_back(config.samples.value > 0 ? TRUE : FALSE);
                result.push_back(WGL_SAMPLES_ARB_);
                result.push_back((std::max)(0, config.samples.value));
            }
            if (config.float_color.requested() &&
                (include_suggested || config.float_color.required())) {
                result.push_back(WGL_PIXEL_TYPE_ARB_);
                result.push_back(config.float_color.value
                    ? WGL_TYPE_RGBA_FLOAT_ARB_ : WGL_TYPE_RGBA_ARB_);
            }
            if (config.single_buffer.requested() &&
                (include_suggested || config.single_buffer.required())) {
                result.push_back(WGL_DOUBLE_BUFFER_ARB_);
                result.push_back(config.single_buffer.value ? FALSE : TRUE);
            }
            if (config.swap.requested() &&
                (include_suggested || config.swap.required())) {
                result.push_back(WGL_SWAP_METHOD_ARB_);
                result.push_back(config.swap.value == swap_method::copy
                    ? WGL_SWAP_COPY_ARB_
                    : config.swap.value == swap_method::flip
                        ? WGL_SWAP_EXCHANGE_ARB_ : WGL_SWAP_UNDEFINED_ARB_);
            }

            // With no effective framebuffer request, preserve alia's historic
            // 32-bit color, 24/8 depth-stencil, double-buffered default.
            if (result.size() == initial_size) {
                result.insert(result.end(), {
                    WGL_COLOR_BITS_ARB_, 32,
                    WGL_DEPTH_BITS_ARB_, 24,
                    WGL_STENCIL_BITS_ARB_, 8,
                    WGL_DOUBLE_BUFFER_ARB_, TRUE,
                });
            }
            result.push_back(0);
            return result;
        }

        int choose_arb_pixel_format(
            HDC hdc,
            const framebuffer_config &config,
            choose_pixel_format_arb_fn choose) {
            for (bool include_suggested : {true, false}) {
                if (!include_suggested && !any_framebuffer_option_requested(config))
                    break;
                auto attributes = pixel_format_attributes(config, include_suggested);
                int format = 0;
                UINT count = 0;
                if (choose(hdc, attributes.data(), nullptr, 1, &format, &count) && count)
                    return format;
            }
            return 0;
        }

        int choose_legacy_pixel_format(HDC hdc, const framebuffer_config &config) {
            PIXELFORMATDESCRIPTOR pfd = {};
            pfd.nSize = sizeof(pfd);
            pfd.nVersion = 1;
            pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
            const bool single = config.single_buffer.requested() && config.single_buffer.value;
            if (!single)
                pfd.dwFlags |= PFD_DOUBLEBUFFER;
            pfd.iPixelType = PFD_TYPE_RGBA;
            pfd.cColorBits = static_cast<BYTE>(config.color_bits.requested()
                ? (std::clamp)(config.color_bits.value, 0, 255) : 32);
            pfd.cRedBits = static_cast<BYTE>((std::clamp)(config.red_bits.value, 0, 255));
            pfd.cGreenBits = static_cast<BYTE>((std::clamp)(config.green_bits.value, 0, 255));
            pfd.cBlueBits = static_cast<BYTE>((std::clamp)(config.blue_bits.value, 0, 255));
            pfd.cAlphaBits = static_cast<BYTE>((std::clamp)(config.alpha_bits.value, 0, 255));
            pfd.cDepthBits = static_cast<BYTE>(config.depth_bits.requested()
                ? (std::clamp)(config.depth_bits.value, 0, 255) : 24);
            pfd.cStencilBits = static_cast<BYTE>(config.stencil_bits.requested()
                ? (std::clamp)(config.stencil_bits.value, 0, 255) : 8);
            pfd.cAuxBuffers = static_cast<BYTE>((std::clamp)(config.aux_buffers.value, 0, 255));
            pfd.cAccumRedBits = static_cast<BYTE>((std::clamp)(config.accum_red_bits.value, 0, 255));
            pfd.cAccumGreenBits = static_cast<BYTE>((std::clamp)(config.accum_green_bits.value, 0, 255));
            pfd.cAccumBlueBits = static_cast<BYTE>((std::clamp)(config.accum_blue_bits.value, 0, 255));
            pfd.cAccumAlphaBits = static_cast<BYTE>((std::clamp)(config.accum_alpha_bits.value, 0, 255));
            pfd.cAccumBits = static_cast<BYTE>((std::min)(255,
                static_cast<int>(pfd.cAccumRedBits) + pfd.cAccumGreenBits +
                pfd.cAccumBlueBits + pfd.cAccumAlphaBits));
            pfd.iLayerType = PFD_MAIN_PLANE;
            return ChoosePixelFormat(hdc, &pfd);
        }

        framebuffer_properties read_properties(
            HDC hdc,
            int format,
            get_pixel_format_attrib_arb_fn get_attribute) {
            framebuffer_properties props;
            PIXELFORMATDESCRIPTOR pfd = {};
            DescribePixelFormat(hdc, format, sizeof(pfd), &pfd);
            props.color_bits = pfd.cColorBits;
            props.red_bits = pfd.cRedBits;
            props.green_bits = pfd.cGreenBits;
            props.blue_bits = pfd.cBlueBits;
            props.alpha_bits = pfd.cAlphaBits;
            props.red_shift = pfd.cRedShift;
            props.green_shift = pfd.cGreenShift;
            props.blue_shift = pfd.cBlueShift;
            props.alpha_shift = pfd.cAlphaShift;
            props.depth_bits = pfd.cDepthBits;
            props.stencil_bits = pfd.cStencilBits;
            props.aux_buffers = pfd.cAuxBuffers;
            props.accum_red_bits = pfd.cAccumRedBits;
            props.accum_green_bits = pfd.cAccumGreenBits;
            props.accum_blue_bits = pfd.cAccumBlueBits;
            props.accum_alpha_bits = pfd.cAccumAlphaBits;
            props.single_buffer = (pfd.dwFlags & PFD_DOUBLEBUFFER) == 0;
            props.swap = (pfd.dwFlags & PFD_SWAP_COPY) != 0
                ? swap_method::copy
                : (pfd.dwFlags & PFD_SWAP_EXCHANGE) != 0
                    ? swap_method::flip : swap_method::undefined;

            int pixel_type = WGL_TYPE_RGBA_ARB_;
            if (get_attribute) {
                const int attributes[]{
                    WGL_COLOR_BITS_ARB_, WGL_RED_BITS_ARB_, WGL_GREEN_BITS_ARB_,
                    WGL_BLUE_BITS_ARB_, WGL_ALPHA_BITS_ARB_, WGL_RED_SHIFT_ARB_,
                    WGL_GREEN_SHIFT_ARB_, WGL_BLUE_SHIFT_ARB_, WGL_ALPHA_SHIFT_ARB_,
                    WGL_DEPTH_BITS_ARB_, WGL_STENCIL_BITS_ARB_, WGL_SAMPLE_BUFFERS_ARB_,
                    WGL_SAMPLES_ARB_, WGL_AUX_BUFFERS_ARB_, WGL_ACCUM_RED_BITS_ARB_,
                    WGL_ACCUM_GREEN_BITS_ARB_, WGL_ACCUM_BLUE_BITS_ARB_,
                    WGL_ACCUM_ALPHA_BITS_ARB_, WGL_DOUBLE_BUFFER_ARB_,
                    WGL_PIXEL_TYPE_ARB_, WGL_SWAP_METHOD_ARB_,
                };
                int values[std::size(attributes)]{};
                if (get_attribute(
                        hdc, format, 0, static_cast<UINT>(std::size(attributes)),
                        attributes, values)) {
                    int i = 0;
                    props.color_bits = values[i++]; props.red_bits = values[i++];
                    props.green_bits = values[i++]; props.blue_bits = values[i++];
                    props.alpha_bits = values[i++]; props.red_shift = values[i++];
                    props.green_shift = values[i++]; props.blue_shift = values[i++];
                    props.alpha_shift = values[i++]; props.depth_bits = values[i++];
                    props.stencil_bits = values[i++]; props.sample_buffers = values[i++];
                    props.samples = props.sample_buffers ? values[i++] : (i++, 0);
                    props.aux_buffers = values[i++]; props.accum_red_bits = values[i++];
                    props.accum_green_bits = values[i++]; props.accum_blue_bits = values[i++];
                    props.accum_alpha_bits = values[i++]; props.single_buffer = values[i++] == FALSE;
                    pixel_type = values[i++];
                    const int swap = values[i++];
                    props.swap = swap == WGL_SWAP_COPY_ARB_ ? swap_method::copy
                        : swap == WGL_SWAP_EXCHANGE_ARB_ ? swap_method::flip
                        : swap_method::undefined;
                }
            }
            props.float_color = pixel_type == WGL_TYPE_RGBA_FLOAT_ARB_;
            props.float_depth = false;
            props.update_display_region = false;
            if (props.float_color)
                props.color_format = pixel_format::rgba_f32;
            else if (props.red_bits == 5 && props.green_bits == 6 && props.blue_bits == 5)
                props.color_format = pixel_format::rgb565;
            else
                props.color_format = pixel_format::bgra8888;
            return props;
        }

        bool apply_swap_interval(vsync_mode mode, bool &actual) {
            const auto set_interval = load_wgl_proc<swap_interval_ext_fn>("wglSwapIntervalEXT");
            const auto get_interval = load_wgl_proc<get_swap_interval_ext_fn>("wglGetSwapIntervalEXT");
            if (!set_interval) {
                actual = false;
                return mode == vsync_mode::suggest;
            }
            const int requested = mode == vsync_mode::disable ? 0 : 1;
            if (!set_interval(requested) && mode != vsync_mode::suggest)
                return false;
            actual = get_interval ? get_interval() != 0 : requested != 0;
            return mode == vsync_mode::suggest || actual == (requested != 0);
        }

        void *win32_ogl_create_context() {
            HWND dummy = create_ogl_dummy_hwnd();
            if (!dummy)
                return nullptr;
            HDC hdc = GetDC(dummy);
            if (!hdc || !set_default_pixel_format(hdc)) {
                if (hdc) ReleaseDC(dummy, hdc);
                DestroyWindow(dummy);
                return nullptr;
            }
            HGLRC hglrc = wglCreateContext(hdc);
            if (!hglrc || !wglMakeCurrent(hdc, hglrc)) {
                if (hglrc) wglDeleteContext(hglrc);
                ReleaseDC(dummy, hdc);
                DestroyWindow(dummy);
                return nullptr;
            }
            auto *context = new win_ogl_ctx;
            context->dummy_hwnd = dummy;
            context->dummy_hdc = hdc;
            context->hglrc = hglrc;
            return context;
        }

        void win32_ogl_destroy_context(void *ctx) {
            if (!ctx)
                return;
            auto *context = static_cast<win_ogl_ctx *>(ctx);
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(context->hglrc);
            ReleaseDC(context->dummy_hwnd, context->dummy_hdc);
            DestroyWindow(context->dummy_hwnd);
            delete context;
        }

        void *win32_ogl_create_surface(
            void *native_handle,
            void *root_ctx,
            const swapchain_desc &desc,
            framebuffer_properties &out) {
            auto *root = static_cast<win_ogl_ctx *>(root_ctx);
            HWND hwnd = static_cast<HWND>(native_handle);
            if (!wglMakeCurrent(root->dummy_hdc, root->hglrc))
                return nullptr;

            const auto choose_arb = load_wgl_proc<choose_pixel_format_arb_fn>(
                "wglChoosePixelFormatARB");
            const auto get_attribute = load_wgl_proc<get_pixel_format_attrib_arb_fn>(
                "wglGetPixelFormatAttribivARB");
            const auto create_attribs = load_wgl_proc<create_context_attribs_arb_fn>(
                "wglCreateContextAttribsARB");

            HDC hdc = GetDC(hwnd);
            if (!hdc)
                return nullptr;
            int format = GetPixelFormat(hdc);
            if (!format) {
                format = choose_arb
                    ? choose_arb_pixel_format(hdc, desc.framebuffer, choose_arb)
                    : 0;
                if (!format)
                    format = choose_legacy_pixel_format(hdc, desc.framebuffer);
                PIXELFORMATDESCRIPTOR pfd = {};
                if (!format || !DescribePixelFormat(hdc, format, sizeof(pfd), &pfd) ||
                    !SetPixelFormat(hdc, format, &pfd)) {
                    ReleaseDC(hwnd, hdc);
                    return nullptr;
                }
            }

            // Sharing contexts whose windows use different pixel formats is
            // implementation-dependent, but is supported by mainstream Windows
            // ICDs. A cross-GPU/ICD incompatibility cleanly fails creation here.
            static constexpr int context_attributes[]{0};
            HGLRC hglrc = create_attribs
                ? create_attribs(hdc, root->hglrc, context_attributes) : nullptr;
            if (!hglrc) {
                hglrc = wglCreateContext(hdc);
                if (!hglrc || !wglShareLists(root->hglrc, hglrc)) {
                    if (hglrc) wglDeleteContext(hglrc);
                    ReleaseDC(hwnd, hdc);
                    return nullptr;
                }
            }
            if (!wglMakeCurrent(hdc, hglrc)) {
                wglDeleteContext(hglrc);
                ReleaseDC(hwnd, hdc);
                return nullptr;
            }

            auto *surface = new win_ogl_surface;
            surface->hwnd = hwnd;
            surface->hdc = hdc;
            surface->hglrc = hglrc;
            surface->pixel_format = format;
            surface->props = read_properties(hdc, format, get_attribute);
            if (!apply_swap_interval(desc.vsync, surface->props.vsync)) {
                wglMakeCurrent(root->dummy_hdc, root->hglrc);
                wglDeleteContext(hglrc);
                ReleaseDC(hwnd, hdc);
                delete surface;
                return nullptr;
            }
            out = surface->props;
            return surface;
        }

        void win32_ogl_destroy_surface(void *value, void *root_ctx) {
            if (!value)
                return;
            auto *surface = static_cast<win_ogl_surface *>(value);
            auto *root = static_cast<win_ogl_ctx *>(root_ctx);
            if (wglGetCurrentContext() == surface->hglrc)
                wglMakeCurrent(root->dummy_hdc, root->hglrc);
            wglDeleteContext(surface->hglrc);
            ReleaseDC(surface->hwnd, surface->hdc);
            delete surface;
        }

        void win32_ogl_make_current(void *value) {
            auto *surface = static_cast<win_ogl_surface *>(value);
            wglMakeCurrent(surface->hdc, surface->hglrc);
        }

        void win32_ogl_make_root_current(void *value) {
            auto *root = static_cast<win_ogl_ctx *>(value);
            wglMakeCurrent(root->dummy_hdc, root->hglrc);
        }

        void win32_ogl_swap_buffers(void *value) {
            auto *surface = static_cast<win_ogl_surface *>(value);
            if (surface->props.single_buffer)
                glFlush();
            else
                SwapBuffers(surface->hdc);
        }

        void win32_ogl_make_none_current() {
            wglMakeCurrent(nullptr, nullptr);
        }
    }

    void register_win32_ogl_platform() {
        register_ogl_platform({
            win32_ogl_create_context,
            win32_ogl_destroy_context,
            win32_ogl_create_surface,
            win32_ogl_destroy_surface,
            win32_ogl_make_current,
            win32_ogl_make_root_current,
            win32_ogl_swap_buffers,
            win32_ogl_make_none_current,
        });
    }
} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
