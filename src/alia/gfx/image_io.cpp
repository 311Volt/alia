#include "image_io.hpp"
#include "stb_image_loaders.hpp"
#include "stb_image_writers.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace alia {

namespace {

	auto& loaders() {
		static std::unordered_map<std::string, image_loader_fn> map;
		return map;
	}

	auto& writers() {
		static std::unordered_map<std::string, image_writer_fn> map;
		return map;
	}

	std::string normalize_ext(std::string_view raw) {
		std::string s(raw);
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
		return s;
	}

	std::string path_extension(std::string_view path) {
		auto ext = std::filesystem::path(path).extension().string();
		if (!ext.empty() && ext[0] == '.')
			ext = ext.substr(1);
		return normalize_ext(ext);
	}

	void register_builtin_loaders() {
		register_stb_image_loaders();
	}

	void register_builtin_writers() {
		register_stb_image_writers();
	}

} // namespace

void register_image_loader(std::string_view extension, image_loader_fn fn) {
	loaders()[normalize_ext(extension)] = std::move(fn);
}

void register_image_writer(std::string_view extension, image_writer_fn fn) {
	writers()[normalize_ext(extension)] = std::move(fn);
}

bitmap load_image(std::string_view path) {
	static std::once_flag flag;
	std::call_once(flag, register_builtin_loaders);

	auto ext = path_extension(path);
	auto it = loaders().find(ext);
	if (it == loaders().end())
		throw std::runtime_error("load_image: no loader registered for extension '" + ext + "'");
	return it->second(path);
}

void save_image(const any_bitmap_view& view, std::string_view path) {
	static std::once_flag flag;
	std::call_once(flag, register_builtin_writers);

	auto ext = path_extension(path);
	auto it = writers().find(ext);
	if (it == writers().end())
		throw std::runtime_error("save_image: no writer registered for extension '" + ext + "'");
	it->second(view, path);
}

} // namespace alia
