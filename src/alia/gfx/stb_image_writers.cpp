#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "stb_image_writers.hpp"
#include "image_io.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace alia {
namespace {

int ldr_comp(pixel_format fmt) {
	switch (fmt) {
		case pixel_format::gray_u8:  return 1;
		case pixel_format::rgb888:   return 3;
		case pixel_format::rgba8888: return 4;
		default: return -1;
	}
}

int hdr_comp(pixel_format fmt) {
	switch (fmt) {
		case pixel_format::gray_f32:  return 1;
		case pixel_format::rgb_f32:   return 3;
		case pixel_format::rgba_f32:  return 4;
		default: return -1;
	}
}

const void* ensure_tight(const any_bitmap_view& view, int bytes_per_pixel, std::vector<std::byte>& buf) {
	const int tight = view.width() * bytes_per_pixel;
	if (view.stride_bytes() == tight)
		return view.line(0);
	buf.resize(static_cast<std::size_t>(tight) * view.height());
	for (int y = 0; y < view.height(); ++y)
		std::memcpy(buf.data() + y * tight, view.line(y), tight);
	return buf.data();
}

void write_png(const any_bitmap_view& view, std::string_view path) {
	int comp = ldr_comp(view.format());
	if (comp < 0)
		throw std::invalid_argument(
			"save_image: PNG writer requires gray_u8, rgb888, or rgba8888 pixel format");
	std::vector<std::byte> buf;
	const void* data = ensure_tight(view, comp, buf);
	std::string p(path);
	if (!stbi_write_png(p.c_str(), view.width(), view.height(), comp, data, view.width() * comp))
		throw std::runtime_error("save_image: stbi_write_png failed for '" + p + "'");
}

void write_jpg(const any_bitmap_view& view, std::string_view path) {
	int comp = ldr_comp(view.format());
	if (comp < 0)
		throw std::invalid_argument(
			"save_image: JPEG writer requires gray_u8, rgb888, or rgba8888 pixel format");
	std::vector<std::byte> buf;
	const void* data = ensure_tight(view, comp, buf);
	std::string p(path);
	if (!stbi_write_jpg(p.c_str(), view.width(), view.height(), comp, data, 90))
		throw std::runtime_error("save_image: stbi_write_jpg failed for '" + p + "'");
}

void write_bmp(const any_bitmap_view& view, std::string_view path) {
	int comp = ldr_comp(view.format());
	if (comp < 0)
		throw std::invalid_argument(
			"save_image: BMP writer requires gray_u8, rgb888, or rgba8888 pixel format");
	std::vector<std::byte> buf;
	const void* data = ensure_tight(view, comp, buf);
	std::string p(path);
	if (!stbi_write_bmp(p.c_str(), view.width(), view.height(), comp, data))
		throw std::runtime_error("save_image: stbi_write_bmp failed for '" + p + "'");
}

void write_tga(const any_bitmap_view& view, std::string_view path) {
	int comp = ldr_comp(view.format());
	if (comp < 0)
		throw std::invalid_argument(
			"save_image: TGA writer requires gray_u8, rgb888, or rgba8888 pixel format");
	std::vector<std::byte> buf;
	const void* data = ensure_tight(view, comp, buf);
	std::string p(path);
	if (!stbi_write_tga(p.c_str(), view.width(), view.height(), comp, data))
		throw std::runtime_error("save_image: stbi_write_tga failed for '" + p + "'");
}

void write_hdr(const any_bitmap_view& view, std::string_view path) {
	int comp = hdr_comp(view.format());
	if (comp < 0)
		throw std::invalid_argument(
			"save_image: HDR writer requires gray_f32, rgb_f32, or rgba_f32 pixel format");
	std::vector<std::byte> buf;
	const void* data = ensure_tight(view, comp * static_cast<int>(sizeof(float)), buf);
	std::string p(path);
	if (!stbi_write_hdr(p.c_str(), view.width(), view.height(), comp, static_cast<const float*>(data)))
		throw std::runtime_error("save_image: stbi_write_hdr failed for '" + p + "'");
}

} // namespace

void register_stb_image_writers() {
	register_image_writer("png",  write_png);
	register_image_writer("jpg",  write_jpg);
	register_image_writer("jpeg", write_jpg);
	register_image_writer("bmp",  write_bmp);
	register_image_writer("tga",  write_tga);
	register_image_writer("hdr",  write_hdr);
}

} // namespace alia
