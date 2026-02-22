#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

#include "gfx_device.hpp"

namespace alia {

// ── Hidden dummy window ───────────────────────────────────────────────

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
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf) return false;
    return SetPixelFormat(hdc, pf, &pfd) == TRUE;
}

// ── OpenGL device impl ────────────────────────────────────────────────

struct ogl_device_impl : gfx_device_impl {
    HWND  dummy_hwnd = nullptr;
    HDC   dummy_hdc  = nullptr;
    HGLRC ctx        = nullptr;

    ~ogl_device_impl() override {
        if (ctx)        { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(ctx); ctx = nullptr; }
        if (dummy_hdc)  { ReleaseDC(dummy_hwnd, dummy_hdc); dummy_hdc = nullptr; }
        if (dummy_hwnd) { DestroyWindow(dummy_hwnd); dummy_hwnd = nullptr; }
    }

    const char* backend_name() const noexcept override { return "opengl"; }
};

// ── OpenGL swapchain impl ─────────────────────────────────────────────

struct ogl_swapchain_impl : swapchain_impl {
    HWND  hwnd = nullptr;
    HDC   hdc  = nullptr;
    HGLRC ctx  = nullptr;  // non-owning; owned by ogl_device_impl
    vec2i size = {};

    ~ogl_swapchain_impl() override {
        if (hdc && hwnd) ReleaseDC(hwnd, hdc);
    }

    void clear(color c) override {
        wglMakeCurrent(hdc, ctx);
        glViewport(0, 0, size.x, size.y);
        glClearColor(c.r, c.g, c.b, c.a);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2) override {
        // Immediate-mode OpenGL 2.x
        glBegin(GL_TRIANGLES);
            glColor3f(v0.col.r, v0.col.g, v0.col.b);
            glVertex2f(v0.position.x, v0.position.y);
            glColor3f(v1.col.r, v1.col.g, v1.col.b);
            glVertex2f(v1.position.x, v1.position.y);
            glColor3f(v2.col.r, v2.col.g, v2.col.b);
            glVertex2f(v2.position.x, v2.position.y);
        glEnd();
    }

    void present() override {
        SwapBuffers(hdc);
    }

    void on_resize(vec2i new_size) override {
        size = new_size;
    }
};

// ── Factory functions ─────────────────────────────────────────────────

static std::unique_ptr<gfx_device_impl> create_ogl_device() {
    HWND dummy = create_ogl_dummy_hwnd();
    if (!dummy) return nullptr;

    HDC hdc = GetDC(dummy);
    if (!hdc) { DestroyWindow(dummy); return nullptr; }

    if (!set_pixel_format(hdc)) {
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    HGLRC ctx = wglCreateContext(hdc);
    if (!ctx) {
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    if (!wglMakeCurrent(hdc, ctx)) {
        wglDeleteContext(ctx);
        ReleaseDC(dummy, hdc);
        DestroyWindow(dummy);
        return nullptr;
    }

    auto impl        = std::make_unique<ogl_device_impl>();
    impl->dummy_hwnd = dummy;
    impl->dummy_hdc  = hdc;
    impl->ctx        = ctx;
    return impl;
}

static std::unique_ptr<swapchain_impl> create_ogl_swapchain(
    gfx_device_impl& dev, void* native_handle, vec2i initial_size)
{
    auto& ogl_dev = static_cast<ogl_device_impl&>(dev);
    HWND hwnd = static_cast<HWND>(native_handle);

    HDC hdc = GetDC(hwnd);
    if (!hdc) return nullptr;

    // SetPixelFormat may only be called once per DC; attempt it
    if (!set_pixel_format(hdc)) {
        // Pixel format might already be set (e.g. second swapchain on same window) — try anyway
        // If it truly fails, wglMakeCurrent will catch it
    }

    if (!wglMakeCurrent(hdc, ogl_dev.ctx)) {
        ReleaseDC(hwnd, hdc);
        return nullptr;
    }

    auto impl   = std::make_unique<ogl_swapchain_impl>();
    impl->hwnd  = hwnd;
    impl->hdc   = hdc;
    impl->ctx   = ogl_dev.ctx;
    impl->size  = initial_size;
    return impl;
}

// ── Registration function ─────────────────────────────────────────────

void register_ogl_backend() {
    register_gfx_backend({
        "opengl",
        gfx_backend::opengl,
        create_ogl_device,
        create_ogl_swapchain,
    });
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
