#ifndef GFX_DEVICE_FEA14748_42F1_4F10_AEAE_DC296E222AF5
#define GFX_DEVICE_FEA14748_42F1_4F10_AEAE_DC296E222AF5

#include "graphics_backend_interface.hpp"
#include <memory>
#include <typeindex>

namespace alia {

    class window;
    class texture;
    class swapchain;

    struct swapchain_config {
        window &target;
    };

    // ── gfx_device ────────────────────────────────────────────────────────

    class gfx_device {
    public:
        gfx_device() = default;
        ~gfx_device();
        gfx_device(gfx_device &&) noexcept;
        gfx_device &operator=(gfx_device &&) noexcept;
        gfx_device(const gfx_device &) = delete;
        gfx_device &operator=(const gfx_device &) = delete;

        static gfx_device create(gfx_backend pref = gfx_backend::auto_);
        [[nodiscard]] swapchain create_swapchain(const swapchain_config &config);

        [[nodiscard]] bool valid() const noexcept { return backend_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return backend_.get(); }
        [[nodiscard]] device_handle *device() const noexcept { return device_; }
        [[nodiscard]] vec2f pixel_center_offset() const;

    private:
        device_handle *device_ = nullptr;
        std::unique_ptr<graphics_backend_interface> backend_;

        explicit gfx_device(device_handle *device, std::unique_ptr<graphics_backend_interface> backend) noexcept
            : device_(device), backend_(std::move(backend)) {}
    };

    // ── swapchain ─────────────────────────────────────────────────────────

    class swapchain {
    public:
        swapchain() = default;
        ~swapchain();
        swapchain(swapchain &&) noexcept;
        swapchain &operator=(swapchain &&) noexcept;
        swapchain(const swapchain &) = delete;
        swapchain &operator=(const swapchain &) = delete;

        [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

        [[nodiscard]] swapchain_handle *handle() const noexcept { return handle_; }
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return backend_; }
        [[nodiscard]] vec2i size() const noexcept { return size_; }

    private:
        friend class gfx_device;
        friend void clear(color c);
        friend void present();
        friend void on_resize(vec2i new_size);
        friend void ensure_current_frame_active();

        swapchain_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        vec2i size_ = {};
        bool frame_active_ = false;

        explicit swapchain(swapchain_handle *handle, const graphics_backend_interface *backend, vec2i size) noexcept
            : handle_(handle), backend_(backend), size_(size) {}
    };

    // ── Thread-locals ─────────────────────────────────────────────────────

    inline thread_local gfx_device *tl_current_device = nullptr;
    inline thread_local swapchain *tl_current_swapchain = nullptr;
    inline thread_local window *tl_current_window = nullptr;
    inline thread_local vec2i tl_current_render_target_size = {};
    inline thread_local int tl_texture_render_target_depth = 0;

    void make_current(gfx_device &d);
    void make_current(swapchain &s);
    void make_current(window &w);

    gfx_device &current_device();
    [[nodiscard]] vec2f current_pixel_center_offset();
    swapchain &current_swapchain();
    window &current_window();
    void ensure_current_frame_active();

    // ── Swapchain free functions ───────────────────────────────────────────

    void clear(color c);
    void present();
    void on_resize(vec2i new_size);

    namespace detail {

        std::size_t register_vertex_definition(std::type_index vertex_type);

        template <vertex_type TVertex>
        [[nodiscard]] const vertex_definition_view &vertex_definition_of() {
            static constexpr auto elements = TVertex::elements();
            static const vertex_definition_view definition{
                register_vertex_definition(typeid(TVertex)),
                static_cast<int>(sizeof(TVertex)),
                elements,
            };
            return definition;
        }

        void draw_prim(
            prim_type type, const void *vertices, int count,
            const vertex_definition_view &definition
        );
        void draw_prim(
            prim_type type, vertex_buffer_handle *vertices, int count,
            const vertex_definition_view &definition
        );
        void draw_indexed_prim(
            prim_type type, const void *vertices, int count,
            std::span<const uint32_t> indices,
            const vertex_definition_view &definition
        );
        void draw_indexed_prim(
            prim_type type, vertex_buffer_handle *vertices, int count,
            index_buffer_handle *indices, int index_count,
            const vertex_definition_view &definition
        );
        void draw_textured_prim(
            prim_type type, const void *vertices, int count,
            const vertex_definition_view &definition, texture &tex
        );
        void draw_textured_prim(
            prim_type type, vertex_buffer_handle *vertices, int count,
            const vertex_definition_view &definition, texture &tex
        );
        void draw_alpha_masked_prim(
            prim_type type, const void *vertices, int count,
            const vertex_definition_view &definition, texture &tex
        );
        void draw_alpha_masked_prim(
            prim_type type, vertex_buffer_handle *vertices, int count,
            const vertex_definition_view &definition, texture &tex
        );
        void draw_textured_indexed_prim(
            prim_type type, const void *vertices, int count,
            std::span<const uint32_t> indices,
            const vertex_definition_view &definition, texture &tex
        );
        void draw_textured_indexed_prim(
            prim_type type, vertex_buffer_handle *vertices, int count,
            index_buffer_handle *indices, int index_count,
            const vertex_definition_view &definition, texture &tex
        );

    } // namespace detail

    // ── Transform / projection matrices ───────────────────────────────────

    void set_current_transform_matrix(std::span<const float, 16> m);
    void get_current_transform_matrix(std::span<float, 16> m);
    void set_current_projection_matrix(std::span<const float, 16> m);
    void get_current_projection_matrix(std::span<float, 16> m);

} // namespace alia

#endif /* GFX_DEVICE_FEA14748_42F1_4F10_AEAE_DC296E222AF5 */
