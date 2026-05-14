#ifndef VERTEX_BUFFER_EB4F0E9F_51CA_465B_8B82_80E927F21794
#define VERTEX_BUFFER_EB4F0E9F_51CA_465B_8B82_80E927F21794

#include "gfx_device.hpp"
#include "vertex.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace alia {

    namespace detail {

        struct vertex_buffer_lock_state {
            vertex_buffer_handle *handle = nullptr;
            const graphics_backend_interface *backend = nullptr;
            buffer_lock_info info = {};
        };

        struct index_buffer_lock_state {
            index_buffer_handle *handle = nullptr;
            const graphics_backend_interface *backend = nullptr;
            buffer_lock_info info = {};
        };

    } // namespace detail

    template <vertex_type TVertex, buffer_lock_mode Mode = buffer_lock_mode::read_write>
    class locked_vertex_buffer_view {
    public:
        using element_type = std::conditional_t<Mode == buffer_lock_mode::read_only, const TVertex, TVertex>;

        locked_vertex_buffer_view() = default;

        ~locked_vertex_buffer_view() {
            release();
        }

        locked_vertex_buffer_view(locked_vertex_buffer_view &&other) noexcept
            : impl_(std::move(other.impl_))
            , view_(other.view_) {
            other.view_ = {};
        }

        locked_vertex_buffer_view &operator=(locked_vertex_buffer_view &&other) noexcept {
            if (this != &other) {
                release();
                impl_ = std::move(other.impl_);
                view_ = other.view_;
                other.view_ = {};
            }
            return *this;
        }

        locked_vertex_buffer_view(const locked_vertex_buffer_view &) = delete;
        locked_vertex_buffer_view &operator=(const locked_vertex_buffer_view &) = delete;

        [[nodiscard]] explicit operator bool() const noexcept {
            return impl_ != nullptr;
        }

        [[nodiscard]] std::span<element_type> view() const noexcept {
            return view_;
        }

        [[nodiscard]] std::span<element_type> operator*() const noexcept {
            return view();
        }

        [[nodiscard]] element_type &operator[](std::size_t index) const noexcept {
            return view_[index];
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return view_.size();
        }

        void release() {
            if (impl_) {
                constexpr bool wrote = (Mode != buffer_lock_mode::read_only);
                impl_->backend->vertex_buffer_unlock.get_or_throw()(impl_->handle, impl_->info, wrote);
                impl_.reset();
                view_ = {};
            }
        }

    private:
        template <vertex_type>
        friend class vertex_buffer;

        std::unique_ptr<detail::vertex_buffer_lock_state> impl_;
        std::span<element_type> view_;

        explicit locked_vertex_buffer_view(std::unique_ptr<detail::vertex_buffer_lock_state> state) noexcept
            : impl_(std::move(state))
            , view_(impl_
                ? std::span<element_type>(
                    reinterpret_cast<element_type *>(impl_->info.data),
                    static_cast<std::size_t>(impl_->info.size_bytes / static_cast<int>(sizeof(TVertex)))
                )
                : std::span<element_type>{}) {}
    };

    template <buffer_lock_mode Mode = buffer_lock_mode::read_write>
    class locked_index_buffer_view {
    public:
        using element_type = std::conditional_t<Mode == buffer_lock_mode::read_only, const uint32_t, uint32_t>;

        locked_index_buffer_view() = default;

        ~locked_index_buffer_view() {
            release();
        }

        locked_index_buffer_view(locked_index_buffer_view &&other) noexcept
            : impl_(std::move(other.impl_))
            , view_(other.view_) {
            other.view_ = {};
        }

        locked_index_buffer_view &operator=(locked_index_buffer_view &&other) noexcept {
            if (this != &other) {
                release();
                impl_ = std::move(other.impl_);
                view_ = other.view_;
                other.view_ = {};
            }
            return *this;
        }

        locked_index_buffer_view(const locked_index_buffer_view &) = delete;
        locked_index_buffer_view &operator=(const locked_index_buffer_view &) = delete;

        [[nodiscard]] explicit operator bool() const noexcept {
            return impl_ != nullptr;
        }

        [[nodiscard]] std::span<element_type> view() const noexcept {
            return view_;
        }

        [[nodiscard]] std::span<element_type> operator*() const noexcept {
            return view();
        }

        [[nodiscard]] element_type &operator[](std::size_t index) const noexcept {
            return view_[index];
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return view_.size();
        }

        void release() {
            if (impl_) {
                constexpr bool wrote = (Mode != buffer_lock_mode::read_only);
                impl_->backend->index_buffer_unlock.get_or_throw()(impl_->handle, impl_->info, wrote);
                impl_.reset();
                view_ = {};
            }
        }

    private:
        friend class index_buffer;

        std::unique_ptr<detail::index_buffer_lock_state> impl_;
        std::span<element_type> view_;

        explicit locked_index_buffer_view(std::unique_ptr<detail::index_buffer_lock_state> state) noexcept
            : impl_(std::move(state))
            , view_(impl_
                ? std::span<element_type>(
                    reinterpret_cast<element_type *>(impl_->info.data),
                    static_cast<std::size_t>(impl_->info.size_bytes / static_cast<int>(sizeof(uint32_t)))
                )
                : std::span<element_type>{}) {}
    };

    template <vertex_type TVertex>
    class vertex_buffer {
    public:
        vertex_buffer() = default;

        vertex_buffer(gfx_device &device, int vertex_count, buffer_usage usage = buffer_usage::dynamic_mesh) {
            create(device, vertex_count, usage, nullptr);
        }

        vertex_buffer(gfx_device &device, std::span<const TVertex> vertices, buffer_usage usage = buffer_usage::static_mesh) {
            create(device, static_cast<int>(vertices.size()), usage, vertices.data());
        }

        ~vertex_buffer() {
            reset();
        }

        vertex_buffer(vertex_buffer &&other) noexcept
            : handle_(std::exchange(other.handle_, nullptr))
            , backend_(std::exchange(other.backend_, nullptr))
            , count_(std::exchange(other.count_, 0))
            , usage_(std::exchange(other.usage_, buffer_usage::static_mesh)) {}

        vertex_buffer &operator=(vertex_buffer &&other) noexcept {
            if (this != &other) {
                reset();
                handle_ = std::exchange(other.handle_, nullptr);
                backend_ = std::exchange(other.backend_, nullptr);
                count_ = std::exchange(other.count_, 0);
                usage_ = std::exchange(other.usage_, buffer_usage::static_mesh);
            }
            return *this;
        }

        vertex_buffer(const vertex_buffer &) = delete;
        vertex_buffer &operator=(const vertex_buffer &) = delete;

        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        [[nodiscard]] int count() const noexcept {
            return count_;
        }

        [[nodiscard]] int stride() const noexcept {
            return static_cast<int>(sizeof(TVertex));
        }

        [[nodiscard]] buffer_usage usage() const noexcept {
            return usage_;
        }

        [[nodiscard]] locked_vertex_buffer_view<TVertex, buffer_lock_mode::read_write>
        lock(int first_vertex = 0, int vertex_count = -1) {
            return locked_vertex_buffer_view<TVertex, buffer_lock_mode::read_write>(
                lock_impl(first_vertex, vertex_count, buffer_lock_mode::read_write)
            );
        }

        [[nodiscard]] locked_vertex_buffer_view<TVertex, buffer_lock_mode::read_only>
        lock_read_only(int first_vertex = 0, int vertex_count = -1) {
            return locked_vertex_buffer_view<TVertex, buffer_lock_mode::read_only>(
                lock_impl(first_vertex, vertex_count, buffer_lock_mode::read_only)
            );
        }

        [[nodiscard]] locked_vertex_buffer_view<TVertex, buffer_lock_mode::write_only>
        lock_write_only(int first_vertex = 0, int vertex_count = -1) {
            return locked_vertex_buffer_view<TVertex, buffer_lock_mode::write_only>(
                lock_impl(first_vertex, vertex_count, buffer_lock_mode::write_only)
            );
        }

        [[nodiscard]] vertex_buffer_handle *impl() noexcept {
            return handle_;
        }

        [[nodiscard]] const vertex_buffer_handle *impl() const noexcept {
            return handle_;
        }

    private:
        void create(gfx_device &device, int vertex_count, buffer_usage usage, const TVertex *initial_data) {
            if (vertex_count <= 0)
                return;

            const auto *backend = device.backend();
            handle_ = backend->create_vertex_buffer.get_or_throw()(
                device.device(),
                static_cast<int>(sizeof(TVertex)),
                vertex_count,
                usage,
                initial_data
            );
            if (handle_) {
                backend_ = backend;
                count_ = vertex_count;
                usage_ = usage;
            }
        }

        void reset() noexcept {
            if (handle_)
                backend_->destroy_vertex_buffer.get_or_throw()(handle_);
            handle_ = nullptr;
            backend_ = nullptr;
            count_ = 0;
            usage_ = buffer_usage::static_mesh;
        }

        std::unique_ptr<detail::vertex_buffer_lock_state>
        lock_impl(int first_vertex, int vertex_count, buffer_lock_mode mode) {
            if (!handle_ || first_vertex < 0 || first_vertex > count_)
                return nullptr;

            const int resolved_count = vertex_count < 0 ? count_ - first_vertex : vertex_count;
            if (resolved_count <= 0 || first_vertex + resolved_count > count_)
                return nullptr;

            buffer_lock_info info{};
            if (!backend_->vertex_buffer_lock.get_or_throw()(handle_, first_vertex, resolved_count, mode, info))
                return nullptr;

            auto state = std::make_unique<detail::vertex_buffer_lock_state>();
            state->handle = handle_;
            state->backend = backend_;
            state->info = info;
            return state;
        }

        vertex_buffer_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        int count_ = 0;
        buffer_usage usage_ = buffer_usage::static_mesh;
    };

    class index_buffer {
    public:
        index_buffer() = default;

        index_buffer(gfx_device &device, int index_count, buffer_usage usage = buffer_usage::dynamic_mesh) {
            create(device, index_count, usage, nullptr);
        }

        index_buffer(gfx_device &device, std::span<const uint32_t> indices, buffer_usage usage = buffer_usage::static_mesh) {
            create(device, static_cast<int>(indices.size()), usage, indices.data());
        }

        ~index_buffer() {
            reset();
        }

        index_buffer(index_buffer &&other) noexcept
            : handle_(std::exchange(other.handle_, nullptr))
            , backend_(std::exchange(other.backend_, nullptr))
            , count_(std::exchange(other.count_, 0))
            , usage_(std::exchange(other.usage_, buffer_usage::static_mesh)) {}

        index_buffer &operator=(index_buffer &&other) noexcept {
            if (this != &other) {
                reset();
                handle_ = std::exchange(other.handle_, nullptr);
                backend_ = std::exchange(other.backend_, nullptr);
                count_ = std::exchange(other.count_, 0);
                usage_ = std::exchange(other.usage_, buffer_usage::static_mesh);
            }
            return *this;
        }

        index_buffer(const index_buffer &) = delete;
        index_buffer &operator=(const index_buffer &) = delete;

        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        [[nodiscard]] int count() const noexcept {
            return count_;
        }

        [[nodiscard]] buffer_usage usage() const noexcept {
            return usage_;
        }

        [[nodiscard]] locked_index_buffer_view<buffer_lock_mode::read_write>
        lock(int first_index = 0, int index_count = -1) {
            return locked_index_buffer_view<buffer_lock_mode::read_write>(
                lock_impl(first_index, index_count, buffer_lock_mode::read_write)
            );
        }

        [[nodiscard]] locked_index_buffer_view<buffer_lock_mode::read_only>
        lock_read_only(int first_index = 0, int index_count = -1) {
            return locked_index_buffer_view<buffer_lock_mode::read_only>(
                lock_impl(first_index, index_count, buffer_lock_mode::read_only)
            );
        }

        [[nodiscard]] locked_index_buffer_view<buffer_lock_mode::write_only>
        lock_write_only(int first_index = 0, int index_count = -1) {
            return locked_index_buffer_view<buffer_lock_mode::write_only>(
                lock_impl(first_index, index_count, buffer_lock_mode::write_only)
            );
        }

        [[nodiscard]] index_buffer_handle *impl() noexcept {
            return handle_;
        }

        [[nodiscard]] const index_buffer_handle *impl() const noexcept {
            return handle_;
        }

    private:
        void create(gfx_device &device, int index_count, buffer_usage usage, const uint32_t *initial_data) {
            if (index_count <= 0)
                return;

            const auto *backend = device.backend();
            handle_ = backend->create_index_buffer.get_or_throw()(device.device(), index_count, usage, initial_data);
            if (handle_) {
                backend_ = backend;
                count_ = index_count;
                usage_ = usage;
            }
        }

        void reset() noexcept {
            if (handle_)
                backend_->destroy_index_buffer.get_or_throw()(handle_);
            handle_ = nullptr;
            backend_ = nullptr;
            count_ = 0;
            usage_ = buffer_usage::static_mesh;
        }

        std::unique_ptr<detail::index_buffer_lock_state>
        lock_impl(int first_index, int index_count, buffer_lock_mode mode) {
            if (!handle_ || first_index < 0 || first_index > count_)
                return nullptr;

            const int resolved_count = index_count < 0 ? count_ - first_index : index_count;
            if (resolved_count <= 0 || first_index + resolved_count > count_)
                return nullptr;

            buffer_lock_info info{};
            if (!backend_->index_buffer_lock.get_or_throw()(handle_, first_index, resolved_count, mode, info))
                return nullptr;

            auto state = std::make_unique<detail::index_buffer_lock_state>();
            state->handle = handle_;
            state->backend = backend_;
            state->info = info;
            return state;
        }

        index_buffer_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        int count_ = 0;
        buffer_usage usage_ = buffer_usage::static_mesh;
    };

} // namespace alia

#endif /* VERTEX_BUFFER_EB4F0E9F_51CA_465B_8B82_80E927F21794 */
