#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "gfx_device.hpp"

namespace alia {

// ── Helpers ───────────────────────────────────────────────────────────

static HWND create_ogl_dummy_hwnd() {
    static const wchar_t* cls = L"AliaDummy_OGL";
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);
        registered = true;
    }
    return CreateWindowExW(0, cls, L"", WS_OVERLAPPEDWINDOW,
                           0, 0, 1, 1,
                           nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

static bool set_pixel_format(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf) return false;
    return SetPixelFormat(hdc, pf, &pfd) == TRUE;
}

// ── Opaque context struct ─────────────────────────────────────────────
// The "context" handle exposed to backend_ogl.cpp is a pointer to this.

struct win_ogl_ctx {
    HWND  dummy_hwnd = nullptr;
    HDC   dummy_hdc  = nullptr;
    HGLRC hglrc      = nullptr;
};

// ── ogl_platform_ops implementations ─────────────────────────────────

static void* win32_ogl_create_context() {
    HWND dummy = create_ogl_dummy_hwnd();
    if (!dummy) return nullptr;

    HDC hdc = GetDC(dummy);
    if (!hdc) { DestroyWindow(dummy); return nullptr; }

    if (!set_pixel_format(hdc)) {
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc) {
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    if (!wglMakeCurrent(hdc, hglrc)) {
        wglDeleteContext(hglrc);
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    auto* c       = new win_ogl_ctx;
    c->dummy_hwnd = dummy;
    c->dummy_hdc  = hdc;
    c->hglrc      = hglrc;
    return static_cast<void*>(c);
}

static void win32_ogl_destroy_context(void* ctx) {
    if (!ctx) return;
    auto* c = static_cast<win_ogl_ctx*>(ctx);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(c->hglrc);
    ReleaseDC(c->dummy_hwnd, c->dummy_hdc);
    DestroyWindow(c->dummy_hwnd);
    delete c;
}

static void* win32_ogl_create_surface(void* native_handle, void* ctx) {
    auto* c   = static_cast<win_ogl_ctx*>(ctx);
    HWND hwnd = static_cast<HWND>(native_handle);

    HDC hdc = GetDC(hwnd);
    if (!hdc) return nullptr;

    // SetPixelFormat may only be called once per DC; attempt it silently
    set_pixel_format(hdc);

    if (!wglMakeCurrent(hdc, c->hglrc)) {
        ReleaseDC(hwnd, hdc);
        return nullptr;
    }

    return static_cast<void*>(hdc);
}

static void win32_ogl_destroy_surface(void* native_handle, void* surface) {
    if (surface && native_handle)
        ReleaseDC(static_cast<HWND>(native_handle), static_cast<HDC>(surface));
}

static void win32_ogl_make_current(void* surface, void* ctx) {
    auto* c = static_cast<win_ogl_ctx*>(ctx);
    wglMakeCurrent(static_cast<HDC>(surface), c->hglrc);
}

static void win32_ogl_swap_buffers(void* surface) {
    SwapBuffers(static_cast<HDC>(surface));
}

static void win32_ogl_make_none_current() {
    wglMakeCurrent(nullptr, nullptr);
}

// ── Registration ──────────────────────────────────────────────────────

void register_win32_ogl_platform() {
    register_ogl_platform({
        win32_ogl_create_context,
        win32_ogl_destroy_context,
        win32_ogl_create_surface,
        win32_ogl_destroy_surface,
        win32_ogl_make_current,
        win32_ogl_swap_buffers,
        win32_ogl_make_none_current,
    });
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
