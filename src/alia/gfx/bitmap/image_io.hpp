#pragma once

#include "bitmap.hpp"
#include <functional>
#include <string_view>

namespace alia {

    using image_loader_fn = std::function<bitmap(std::string_view path)>;
    using image_writer_fn = std::function<void(const any_bitmap_view &view, std::string_view path)>;

    /// Register a loader for the given file extension (without leading dot, e.g. "png").
    /// Extension matching is case-insensitive. A later registration for the same
    /// extension replaces the earlier one.
    void register_image_loader(std::string_view extension, image_loader_fn fn);

    /// Register a writer for the given file extension (without leading dot, e.g. "png").
    /// Extension matching is case-insensitive. A later registration for the same
    /// extension replaces the earlier one.
    void register_image_writer(std::string_view extension, image_writer_fn fn);

    /// Load an image from @p path, dispatching to the registered loader for its extension.
    /// @throws std::runtime_error if no loader is registered for the extension or the load fails.
    bitmap load_image(std::string_view path);

    /// Save @p view to @p path, dispatching to the registered writer for its extension.
    /// @throws std::runtime_error   if no writer is registered for the extension or the save fails.
    /// @throws std::invalid_argument if the view's pixel format is not supported by the writer.
    void save_image(const any_bitmap_view &view, std::string_view path);

    /// RAII helper for registering an image loader during static initialization.
    struct image_loader_registrar {
        image_loader_registrar(std::string_view ext, image_loader_fn fn) { register_image_loader(ext, std::move(fn)); }
    };

    /// RAII helper for registering an image writer during static initialization.
    struct image_writer_registrar {
        image_writer_registrar(std::string_view ext, image_writer_fn fn) { register_image_writer(ext, std::move(fn)); }
    };

} // namespace alia
