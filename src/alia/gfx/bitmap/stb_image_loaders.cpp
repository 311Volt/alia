#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "stb_image_loaders.hpp"
#include "image_io.hpp"
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace alia {
    namespace {

        bitmap load_stb_ldr(std::string_view path) {
            std::string pathstr(path);
            int w, h, channels;
            unsigned char *data = stbi_load(pathstr.c_str(), &w, &h, &channels, 0);
            if (!data)
                throw std::runtime_error("load_image: stb_image failed to load '" + pathstr + "': " + stbi_failure_reason());

            std::unique_ptr<unsigned char, decltype(&stbi_image_free)> guard(data, stbi_image_free);

            auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte *>(data), static_cast<std::size_t>(w) * h * channels);

            switch (channels) {
            case 1:  return bitmap(pixel_format::gray_u8, {w, h}, bytes, w * 1);
            case 3:  return bitmap(pixel_format::rgb888, {w, h}, bytes, w * 3);
            case 4:  return bitmap(pixel_format::rgba8888, {w, h}, bytes, w * 4);
            default: throw std::runtime_error("load_image: stb_image returned unsupported channel count " + std::to_string(channels));
            }
        }

        bitmap load_stb_hdr(std::string_view path) {
            std::string pathstr(path);
            int w, h, channels;
            float *data = stbi_loadf(pathstr.c_str(), &w, &h, &channels, 0);
            if (!data)
                throw std::runtime_error("load_image: stb_image failed to load HDR '" + pathstr + "': " + stbi_failure_reason());

            std::unique_ptr<float, decltype(&stbi_image_free)> guard(data, stbi_image_free);

            auto bytes = std::span<const std::byte>(
                reinterpret_cast<const std::byte *>(data), static_cast<std::size_t>(w) * h * channels * sizeof(float)
            );

            switch (channels) {
            case 1:  return bitmap(pixel_format::gray_f32, {w, h}, bytes, w * 4);
            case 3:  return bitmap(pixel_format::rgb_f32, {w, h}, bytes, w * 12);
            case 4:  return bitmap(pixel_format::rgba_f32, {w, h}, bytes, w * 16);
            default: throw std::runtime_error("load_image: stb_image returned unsupported HDR channel count " + std::to_string(channels));
            }
        }

    } // namespace

    void register_stb_image_loaders() {
        register_image_loader("png", load_stb_ldr);
        register_image_loader("jpg", load_stb_ldr);
        register_image_loader("jpeg", load_stb_ldr);
        register_image_loader("bmp", load_stb_ldr);
        register_image_loader("tga", load_stb_ldr);
        register_image_loader("gif", load_stb_ldr);
        register_image_loader("psd", load_stb_ldr);
        register_image_loader("pnm", load_stb_ldr);
        register_image_loader("ppm", load_stb_ldr);
        register_image_loader("pgm", load_stb_ldr);
        register_image_loader("pic", load_stb_ldr);
        register_image_loader("hdr", load_stb_hdr);
    }

} // namespace alia
