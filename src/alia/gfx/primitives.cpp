#include "primitives.hpp"
#include "gfx_device.hpp"

namespace alia {

void clear(color c) {
    current_swapchain().clear(c);
}

void present() {
    current_swapchain().present();
}

void fill_rect(rect_f r, color c) {
    colored_vertex v0{{r.left(), r.top()}, c};
    colored_vertex v1{{r.right(), r.top()}, c};
    colored_vertex v2{{r.left(), r.bottom()}, c};
    colored_vertex v3{{r.right(), r.bottom()}, c};
    draw_triangle(v0, v1, v2);
    draw_triangle(v1, v3, v2);
}

void draw_line(vec2f a, vec2f b, color c, float thickness) {
    vec2f dir = b - a;
    float len = dir.length();
    if (len == 0) return;
    dir /= len;
    vec2f perp{-dir.y, dir.x};
    vec2f offset = perp * (thickness * 0.5f);

    colored_vertex v0{a + offset, c};
    colored_vertex v1{b + offset, c};
    colored_vertex v2{a - offset, c};
    colored_vertex v3{b - offset, c};

    draw_triangle(v0, v1, v2);
    draw_triangle(v1, v3, v2);
}

void draw_rect(rect_f r, color c, float thickness) {
    vec2f tl = r.top_left();
    vec2f tr = r.top_right();
    vec2f bl = r.bottom_left();
    vec2f br = r.bottom_right();

    float h = thickness * 0.5f;

    draw_line({tl.x - h, tl.y}, {tr.x + h, tr.y}, c, thickness);
    draw_line({tr.x, tr.y - h}, {br.x, br.y + h}, c, thickness);
    draw_line({br.x + h, br.y}, {bl.x - h, bl.y}, c, thickness);
    draw_line({bl.x, bl.y + h}, {tl.x, tl.y - h}, c, thickness);
}

} // namespace alia
