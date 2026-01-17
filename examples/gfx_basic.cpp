#include <alia/alia.hpp>
#include <alia/graphics/gfx_basic.hpp>

#define ALIA_BACKEND_GFX_BASIC_USE_SDL2
#include <alia/graphics/gfx_basic.hpp>

#include <SDL.h>
#include <SDL_main.h>

#include <iostream>

using namespace alia;

// A simple helper function to convert degrees to radians for rotation.
double deg2rad(double degrees) { return degrees * 3.14159265358979323846 / 180.0; }

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("alia/graphics/basic demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    backend::sdl2::graphics_basic_backend_sdl2 backend(renderer);
    graphics_context_basic gfx(backend);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        gfx.clear_canvas({0, 0, 0, 255});

        // Draw points
        std::vector<vec2i> points;
        for (int i = 0; i < 10; ++i) {
            points.push_back({10 + i * 10, 10});
        }
        gfx.draw_points(points, {255, 0, 0, 255});

        // Draw lines
        gfx.draw_line({20, 20}, {120, 120}, {0, 255, 0, 255});

        // Draw rect
        gfx.draw_rect({{150, 20}, {250, 120}}, {0, 0, 255, 255});

        // Fill rect
        gfx.fill_rect({{280, 20}, {380, 120}}, {255, 255, 0, 255});

        // Draw triangle
        std::array<vec2i, 3> triangle = {{{400, 120}, {450, 20}, {500, 120}}};
        gfx.draw_triangle(triangle, {255, 0, 255, 255});

        // Draw a simple image
        image img(pixel_format::rgba8888, {50, 50});
        img.for_each<pixel_format::rgba8888>([&](pixel<pixel_format::rgba8888> &p, vec2i pos) {
            p.r = pos.x * 5;
            p.g = pos.y * 5;
            p.b = 128;
            p.a = 255;
        });
        gfx.draw_image(img, {20, 150});

        // Draw an advanced image
        draw_image_params params;
        params.dst_pos = {100, 150};
        params.dst_size = {100, 100};
        params.tint = color::of_rgb24(0x8080FF);
        params.rotation = deg2rad(30);
        gfx.draw_image_advanced(img, params);

        gfx.flip_canvas();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
