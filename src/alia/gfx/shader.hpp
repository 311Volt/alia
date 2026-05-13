#ifndef SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8
#define SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8

#include "alia/core/color.hpp"
#include "alia/gfx/gfx_device.hpp"
#include "alia/gfx/transform.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace alia {

    class texture;

    namespace detail {

        template <typename T>
        struct shader_vec_traits {
            static constexpr int size = 0;
            using value_type = void;
        };

        template <typename T>
        struct shader_vec_traits<vec2<T>> {
            static constexpr int size = 2;
            using value_type = T;
        };

        template <typename T>
        struct shader_vec_traits<vec3<T>> {
            static constexpr int size = 3;
            using value_type = T;
        };

        template <typename T>
        struct shader_vec_traits<vec4<T>> {
            static constexpr int size = 4;
            using value_type = T;
        };

        template <typename T>
        struct is_matrix_span : std::false_type {};

        template <>
        struct is_matrix_span<std::span<float, 16>> : std::true_type {};

        template <>
        struct is_matrix_span<std::span<const float, 16>> : std::true_type {};

        template <typename T>
        inline constexpr bool is_supported_scalar_v =
            (std::is_integral_v<T> || std::is_floating_point_v<T>) &&
            !std::is_same_v<T, bool>;

    } // namespace detail

    template <typename T>
    concept scalar_shader_constant_value =
        detail::is_supported_scalar_v<std::remove_cvref_t<T>>;

    template <typename T>
    concept vector_shader_constant_value =
        detail::shader_vec_traits<std::remove_cvref_t<T>>::size != 0 &&
        detail::is_supported_scalar_v<
            typename detail::shader_vec_traits<std::remove_cvref_t<T>>::value_type>;

    template <typename T>
    concept matrix_shader_constant_value =
        std::same_as<std::remove_cvref_t<T>, transform> ||
        detail::is_matrix_span<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept shader_constant_value =
        scalar_shader_constant_value<T> ||
        vector_shader_constant_value<T> ||
        matrix_shader_constant_value<T> ||
        std::same_as<std::remove_cvref_t<T>, color>;

    namespace detail {

        struct shader_constant_payload_storage {
            std::array<float, 16> floats = {};
            std::array<int, 4> ints = {};
            shader_constant_value_type type = shader_constant_value_type::float_1;
            std::size_t float_count = 0;
            std::size_t int_count = 0;

            [[nodiscard]] shader_constant_payload payload() const noexcept {
                return {
                    type,
                    std::span<const float>(floats.data(), float_count),
                    std::span<const int>(ints.data(), int_count)
                };
            }
        };

        template <typename T>
        [[nodiscard]] shader_constant_payload_storage make_shader_constant_payload(const T &value) {
            using raw_t = std::remove_cvref_t<T>;

            shader_constant_payload_storage storage;
            if constexpr (std::is_integral_v<raw_t>) {
                storage.ints[0] = static_cast<int>(value);
                storage.type = shader_constant_value_type::int_1;
                storage.int_count = 1;
            } else if constexpr (std::is_floating_point_v<raw_t>) {
                storage.floats[0] = static_cast<float>(value);
                storage.type = shader_constant_value_type::float_1;
                storage.float_count = 1;
            } else if constexpr (std::same_as<raw_t, color>) {
                storage.floats[0] = value.r;
                storage.floats[1] = value.g;
                storage.floats[2] = value.b;
                storage.floats[3] = value.a;
                storage.type = shader_constant_value_type::float_4;
                storage.float_count = 4;
            } else if constexpr (vector_shader_constant_value<raw_t>) {
                constexpr int n = shader_vec_traits<raw_t>::size;
                using scalar_t = typename shader_vec_traits<raw_t>::value_type;
                if constexpr (std::is_integral_v<scalar_t>) {
                    if constexpr (n >= 1)
                        storage.ints[0] = static_cast<int>(value.x);
                    if constexpr (n >= 2)
                        storage.ints[1] = static_cast<int>(value.y);
                    if constexpr (n >= 3)
                        storage.ints[2] = static_cast<int>(value.z);
                    if constexpr (n >= 4)
                        storage.ints[3] = static_cast<int>(value.w);
                    storage.type =
                        n == 2 ? shader_constant_value_type::int_2 :
                        n == 3 ? shader_constant_value_type::int_3 :
                                 shader_constant_value_type::int_4;
                    storage.int_count = n;
                } else {
                    if constexpr (n >= 1)
                        storage.floats[0] = static_cast<float>(value.x);
                    if constexpr (n >= 2)
                        storage.floats[1] = static_cast<float>(value.y);
                    if constexpr (n >= 3)
                        storage.floats[2] = static_cast<float>(value.z);
                    if constexpr (n >= 4)
                        storage.floats[3] = static_cast<float>(value.w);
                    storage.type =
                        n == 2 ? shader_constant_value_type::float_2 :
                        n == 3 ? shader_constant_value_type::float_3 :
                                 shader_constant_value_type::float_4;
                    storage.float_count = n;
                }
            } else if constexpr (std::same_as<raw_t, transform>) {
                const auto *src = &value.m[0][0];
                std::copy(src, src + 16, storage.floats.begin());
                storage.type = shader_constant_value_type::matrix_4x4;
                storage.float_count = 16;
            } else {
                std::copy(value.begin(), value.end(), storage.floats.begin());
                storage.type = shader_constant_value_type::matrix_4x4;
                storage.float_count = 16;
            }
            return storage;
        }

    } // namespace detail

    template <shader_constant_value TValue>
    class shader_constant {
    public:
        shader_constant() = default;

        [[nodiscard]] explicit operator bool() const noexcept {
            return backend_ != nullptr && program_ != nullptr && slot_.valid;
        }

        void set_value(const TValue &value) const {
            if (!*this)
                throw shader_error("shader_constant::set_value: invalid shader constant");
            auto storage = detail::make_shader_constant_payload(value);
            auto payload = storage.payload();
            backend_->shader_set_constant.get_or_throw()(program_, slot_, payload);
        }

    private:
        friend class shader_program;

        shader_constant(
            const graphics_backend_interface *backend,
            shader_program_handle *program,
            shader_constant_slot slot
        ) noexcept
            : backend_(backend), program_(program), slot_(slot) {}

        const graphics_backend_interface *backend_ = nullptr;
        shader_program_handle *program_ = nullptr;
        shader_constant_slot slot_ = {};
    };

    class shader_sampler {
    public:
        shader_sampler() = default;

        [[nodiscard]] explicit operator bool() const noexcept {
            return backend_ != nullptr && program_ != nullptr && slot_.valid;
        }

        void set_texture(texture &tex) const;

    private:
        friend class shader_program;

        shader_sampler(
            const graphics_backend_interface *backend,
            shader_program_handle *program,
            shader_sampler_slot slot
        ) noexcept
            : backend_(backend), program_(program), slot_(slot) {}

        const graphics_backend_interface *backend_ = nullptr;
        shader_program_handle *program_ = nullptr;
        shader_sampler_slot slot_ = {};
    };

    class shader_program {
    public:
        shader_program() = default;
        shader_program(gfx_device &device, const shader_program_desc &desc);
        ~shader_program();
        shader_program(shader_program &&other) noexcept;
        shader_program &operator=(shader_program &&other) noexcept;
        shader_program(const shader_program &) = delete;
        shader_program &operator=(const shader_program &) = delete;

        [[nodiscard]] bool valid() const noexcept {
            return handle_ != nullptr;
        }
        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        template <shader_constant_value TValue>
        [[nodiscard]] shader_constant<TValue>
        allocate_constant(std::string_view identifier, shader_type stage = shader_type::vertex) {
            if (!handle_)
                throw shader_error("shader_program::allocate_constant: program is not valid");
            shader_constant_slot slot =
                backend_->shader_lookup_constant.get_or_throw()(handle_, identifier, stage);
            if (!slot.valid)
                throw shader_error("shader_program::allocate_constant: shader constant not found");
            return shader_constant<TValue>(backend_, handle_, slot);
        }

        [[nodiscard]] shader_sampler
        allocate_sampler(std::string_view identifier, shader_type stage = shader_type::pixel);

        [[nodiscard]] shader_program_handle *impl() noexcept {
            return handle_;
        }
        [[nodiscard]] const shader_program_handle *impl() const noexcept {
            return handle_;
        }
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept {
            return backend_;
        }

    private:
        shader_program_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
    };

    void use(shader_program &program);
    void reset_shader() noexcept;
    [[nodiscard]] shader_program_handle *current_shader_program_handle() noexcept;

} // namespace alia

#endif /* SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8 */
