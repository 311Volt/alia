#ifndef GFX_DEVICE_FEA14748_42F1_4F10_AEAE_DC296E222AF5
#define GFX_DEVICE_FEA14748_42F1_4F10_AEAE_DC296E222AF5

#include "graphics_backend_interface.hpp"

#include <memory>
#include <typeindex>

namespace alia {

    class window;
    class frame;
    class swapchain;

    struct swapchain_config { window &target; };

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

    class swapchain {
    public:
        swapchain() = default;
        ~swapchain();
        swapchain(swapchain &&) noexcept;
        swapchain &operator=(swapchain &&) noexcept;
        swapchain(const swapchain &) = delete;
        swapchain &operator=(const swapchain &) = delete;

        [[nodiscard]] frame begin_frame();
        void on_resize(vec2i new_size);
        [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] swapchain_handle *handle() const noexcept { return handle_; }
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return backend_; }
        [[nodiscard]] vec2i size() const noexcept { return size_; }

    private:
        friend class gfx_device;
        friend class frame;
        swapchain_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        device_handle *device_ = nullptr;
        vec2i size_ = {};
        bool frame_active_ = false;
        explicit swapchain(swapchain_handle *handle, const graphics_backend_interface *backend, device_handle *device, vec2i size) noexcept
            : handle_(handle), backend_(backend), device_(device), size_(size) {}
    };

    inline thread_local gfx_device *tl_current_device = nullptr;
    inline thread_local window *tl_current_window = nullptr;

    void make_current(gfx_device &d);
    void make_current(window &w);
    gfx_device &current_device();
    [[nodiscard]] vec2f current_pixel_center_offset();
    window &current_window();

    namespace detail {
        std::size_t register_vertex_definition(std::type_index vertex_type);

        template <vertex_type TVertex>
        [[nodiscard]] const vertex_definition_view &vertex_definition_of() {
            static constexpr auto elements = TVertex::elements();
            static const vertex_definition_view definition{
                register_vertex_definition(typeid(TVertex)), static_cast<int>(sizeof(TVertex)), elements};
            return definition;
        }
    } // namespace detail

} // namespace alia

#endif
