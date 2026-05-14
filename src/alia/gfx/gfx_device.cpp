#include "gfx_device.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "../os/window.hpp"
#include <array>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
namespace alia {
    void register_d3d9_backend();
}
#endif

#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
namespace alia {
    void register_ogl_backend();
}
#endif

namespace alia {

    namespace {

        bool has_vertex_color(std::span<const vertex_element> elements) {
            for (const auto &e : elements) {
                if (e.attribute == vertex_attr::color_attr)
                    return true;
            }
            return false;
        }

        int compute_primitive_count(prim_type type, int vertex_or_index_count) {
            if (vertex_or_index_count < 3)
                return 0;

            switch (type) {
            case prim_type::triangle_list:  return vertex_or_index_count / 3;
            case prim_type::triangle_strip: return vertex_or_index_count - 2;
            case prim_type::triangle_fan:   return vertex_or_index_count - 2;
            }
            return 0;
        }

        fixed_function_texture_mode choose_fixed_function_texture_mode(
            texture_handle *texture,
            bool alpha_masked,
            std::span<const vertex_element> elements
        ) {
            if (!texture)
                return fixed_function_texture_mode::vertex_color;
            if (alpha_masked)
                return fixed_function_texture_mode::alpha_mask;
            return has_vertex_color(elements)
                ? fixed_function_texture_mode::texture_modulate_vertex_color
                : fixed_function_texture_mode::texture_replace;
        }

        void bind_primary_texture(
            const graphics_backend_interface *backend,
            device_handle *device,
            texture_handle *texture,
            const sampler_state &sampler
        ) {
            backend->bind_texture.get_or_throw()(device, texture_binding{0, texture});
            if (texture)
                backend->set_texture_sampler.get_or_throw()(device, texture_sampler_binding{0, texture, sampler});
        }

        void apply_fixed_function_state(
            const graphics_backend_interface *backend,
            device_handle *device,
            texture_handle *texture,
            const sampler_state &sampler,
            fixed_function_texture_mode texture_mode
        ) {
            std::array<float, 16> world = {};
            std::array<float, 16> projection = {};
            get_current_transform_matrix(std::span<float, 16>(world.data(), 16));
            get_current_projection_matrix(std::span<float, 16>(projection.data(), 16));

            backend->bind_shader_program.get_or_throw()(device, nullptr);
            backend->set_fixed_function_matrices.get_or_throw()(
                device,
                fixed_function_matrices{
                    std::span<const float, 16>(world.data(), 16),
                    std::span<const float, 16>(projection.data(), 16)
                }
            );
            bind_primary_texture(backend, device, texture, sampler);
            backend->set_fixed_function_texture_mode.get_or_throw()(device, texture_mode);
        }

        void apply_shader_state(
            const graphics_backend_interface *backend,
            device_handle *device,
            shader_program_handle *shader,
            texture_handle *texture,
            const sampler_state &sampler
        ) {
            backend->bind_shader_program.get_or_throw()(device, shader);
            backend->apply_shader_constants.get_or_throw()(device, shader);
            backend->apply_shader_samplers.get_or_throw()(device, shader);
            if (texture)
                bind_primary_texture(backend, device, texture, sampler);
        }

        void submit_draw(
            prim_type type,
            const void *vertices,
            int vertex_count,
            int vertex_stride,
            std::span<const uint32_t> indices,
            bool indexed,
            std::type_index vertex_type,
            std::span<const vertex_element> elements,
            texture *primary_texture,
            bool alpha_masked
        ) {
            if (!vertices || vertex_count <= 0 || vertex_stride <= 0)
                return;

            const int count_for_primitives = indexed ? static_cast<int>(indices.size()) : vertex_count;
            const int primitive_count = compute_primitive_count(type, count_for_primitives);
            if (primitive_count <= 0)
                return;
            if (indexed && indices.empty())
                return;

            texture_handle *texture_handle = primary_texture ? primary_texture->impl() : nullptr;
            if (primary_texture && !texture_handle)
                return;

            sampler_state sampler = {};
            if (primary_texture)
                sampler = primary_texture->sampler();

            auto &device = current_device();
            const auto *backend = device.backend();
            device_handle *dev = device.device();
            shader_program_handle *shader = current_shader_program_handle();

            backend->set_render_state.get_or_throw()(dev, render_state{});
            backend->set_blend_state.get_or_throw()(
                dev,
                blend_state{
                    true,
                    blend_factor::src_alpha,
                    blend_factor::inv_src_alpha,
                    blend_op::add
                }
            );

            const vertex_input_mode input_mode =
                shader ? vertex_input_mode::shader_attributes : vertex_input_mode::fixed_function;
            const vertex_input_binding input{
                input_mode,
                vertices,
                vertex_stride,
                vertex_type,
                elements
            };

            if (shader) {
                apply_shader_state(backend, dev, shader, texture_handle, sampler);
            } else {
                apply_fixed_function_state(
                    backend,
                    dev,
                    texture_handle,
                    sampler,
                    choose_fixed_function_texture_mode(texture_handle, alpha_masked, elements)
                );
            }

            backend->bind_vertex_input.get_or_throw()(dev, input);
            if (indexed) {
                backend->draw_elements_immediate.get_or_throw()(
                    dev,
                    draw_elements_immediate{
                        type,
                        vertices,
                        vertex_count,
                        vertex_stride,
                        indices,
                        primitive_count
                    }
                );
            } else {
                backend->draw_arrays_immediate.get_or_throw()(
                    dev,
                    draw_arrays_immediate{
                        type,
                        vertices,
                        vertex_count,
                        vertex_stride,
                        primitive_count
                    }
                );
            }
            backend->unbind_vertex_input.get_or_throw()(dev, input);
        }

    } // namespace

    // ── Backend registry ──────────────────────────────────────────────────

    static std::vector<gfx_backend_factory> &backend_registry() {
        static std::vector<gfx_backend_factory> reg;
        return reg;
    }

    void register_gfx_backend(gfx_backend_factory factory) {
        backend_registry().push_back(factory);
    }

    static void init_gfx_backends() {
        static std::once_flag flag;
        std::call_once(flag, []() {
#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9
            register_d3d9_backend();
#endif
#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL
            register_ogl_backend();
#endif
        });
    }

    // ── Thread-local current-object setters ───────────────────────────────

    void make_current(gfx_device &d) {
        tl_current_device = &d;
    }
    void make_current(swapchain &s) {
        tl_current_swapchain = &s;
    }
    void make_current(window &w) {
        tl_current_window = &w;
    }

    gfx_device &current_device() {
        if (!tl_current_device)
            throw std::runtime_error("No current gfx_device");
        return *tl_current_device;
    }

    swapchain &current_swapchain() {
        if (!tl_current_swapchain)
            throw std::runtime_error("No current swapchain");
        return *tl_current_swapchain;
    }

    window &current_window() {
        if (!tl_current_window)
            throw std::runtime_error("No current window");
        return *tl_current_window;
    }

    // ── gfx_device ────────────────────────────────────────────────────────

    gfx_device::~gfx_device() {
        if (device_)
            backend_->destroy_device.get_or_throw()(device_);
        if (tl_current_device == this)
            tl_current_device = nullptr;
    }

    gfx_device::gfx_device(gfx_device &&other) noexcept
        : device_(std::exchange(other.device_, nullptr))
        , backend_(std::move(other.backend_)) {
        if (tl_current_device == &other)
            tl_current_device = this;
    }

    gfx_device &gfx_device::operator=(gfx_device &&other) noexcept {
        if (this != &other) {
            if (device_)
                backend_->destroy_device.get_or_throw()(device_);
            device_ = std::exchange(other.device_, nullptr);
            backend_ = std::move(other.backend_);
            if (tl_current_device == &other)
                tl_current_device = this;
        }
        return *this;
    }

    vec2f gfx_device::pixel_center_offset() const {
        if (!valid())
            throw std::runtime_error("gfx_device::pixel_center_offset: device is not valid");
        return backend_->pixel_center_offset;
    }

    gfx_device gfx_device::create(gfx_backend pref) {
        init_gfx_backends();

        auto try_factory = [](const gfx_backend_factory &f) -> gfx_device {
            auto [handle, iface] = f.create();
            if (!handle)
                return {};
            auto backend = std::make_unique<graphics_backend_interface>(std::move(iface));
            gfx_device d(handle, std::move(backend));
            make_current(d);
            return d;
        };

        if (pref != gfx_backend::auto_) {
            for (const auto &f : backend_registry()) {
                if (f.id != pref)
                    continue;
                if (auto d = try_factory(f); d.valid())
                    return d;
            }
        }

        for (const auto &f : backend_registry()) {
            if (auto d = try_factory(f); d.valid())
                return d;
        }

        throw std::runtime_error("gfx_device: no graphics backend available");
    }

    vec2f current_pixel_center_offset() {
        return current_device().pixel_center_offset();
    }

    swapchain gfx_device::create_swapchain(const swapchain_config &config) {
        if (!valid())
            throw std::runtime_error("gfx_device::create_swapchain: device is not valid");

        swapchain_handle *sc = backend_->create_swapchain.get_or_throw()(
            device_, config.target.native_handle(), config.target.size()
        );
        if (!sc)
            throw std::runtime_error("gfx_device::create_swapchain: failed to create swapchain");

        swapchain s(sc, backend_.get(), config.target.size());
        make_current(s);
        make_current(config.target);
        return s;
    }

    // ── swapchain ─────────────────────────────────────────────────────────

    swapchain::~swapchain() {
        if (handle_ && frame_active_)
            backend_->swapchain_end_frame.get_or_throw()(handle_);
        if (handle_)
            backend_->destroy_swapchain.get_or_throw()(handle_);
        if (tl_current_swapchain == this)
            tl_current_swapchain = nullptr;
    }

    swapchain::swapchain(swapchain &&other) noexcept
        : handle_(std::exchange(other.handle_, nullptr))
        , backend_(std::exchange(other.backend_, nullptr))
        , size_(std::exchange(other.size_, {}))
        , frame_active_(std::exchange(other.frame_active_, false)) {
        if (tl_current_swapchain == &other)
            tl_current_swapchain = this;
    }

    swapchain &swapchain::operator=(swapchain &&other) noexcept {
        if (this != &other) {
            if (handle_ && frame_active_)
                backend_->swapchain_end_frame.get_or_throw()(handle_);
            if (handle_)
                backend_->destroy_swapchain.get_or_throw()(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
            backend_ = std::exchange(other.backend_, nullptr);
            size_ = std::exchange(other.size_, {});
            frame_active_ = std::exchange(other.frame_active_, false);
            if (tl_current_swapchain == &other)
                tl_current_swapchain = this;
        }
        return *this;
    }

    // ── Swapchain free functions ───────────────────────────────────────────

    void clear(color c) {
        auto &sc = current_swapchain();
        auto &dev = current_device();
        const auto *backend = sc.backend();

        if (!sc.frame_active_) {
            backend->swapchain_begin_frame.get_or_throw()(sc.handle());
            sc.frame_active_ = true;
        }

        backend->set_render_state.get_or_throw()(dev.device(), render_state{});
        backend->set_blend_state.get_or_throw()(dev.device(), blend_state{});
        backend->set_viewport.get_or_throw()(
            dev.device(),
            render_viewport{
                {},
                sc.size(),
                0.0f,
                1.0f
            }
        );
        backend->clear_render_target.get_or_throw()(dev.device(), c);
    }

    void present() {
        auto &sc = current_swapchain();
        if (sc.frame_active_) {
            sc.backend()->swapchain_end_frame.get_or_throw()(sc.handle());
            sc.frame_active_ = false;
        }
        sc.backend()->swapchain_present.get_or_throw()(sc.handle());
    }

    void on_resize(vec2i new_size) {
        auto &sc = current_swapchain();
        sc.backend()->swapchain_on_resize.get_or_throw()(sc.handle(), new_size);
        sc.size_ = new_size;
    }

    // ── Draw free functions ───────────────────────────────────────────────

    void draw_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements
    ) {
        submit_draw(type, vertices, count, stride, {}, false, vtx_type, elements, nullptr, false);
    }

    void draw_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements
    ) {
        submit_draw(type, vertices, count, stride, indices, true, vtx_type, elements, nullptr, false);
    }

    void draw_textured_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture &tex
    ) {
        submit_draw(type, vertices, count, stride, {}, false, vtx_type, elements, &tex, false);
    }

    void draw_alpha_masked_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture &tex
    ) {
        submit_draw(type, vertices, count, stride, {}, false, vtx_type, elements, &tex, true);
    }

    void draw_textured_indexed_prim(
        prim_type type, const void *vertices, int count, int stride,
        std::span<const uint32_t> indices,
        std::type_index vtx_type, std::span<const vertex_element> elements,
        texture &tex
    ) {
        submit_draw(type, vertices, count, stride, indices, true, vtx_type, elements, &tex, false);
    }

} // namespace alia
