#ifndef ALIA_GFX_PIPELINE_HPP
#define ALIA_GFX_PIPELINE_HPP

#include "gfx_device.hpp"
#include "shader.hpp"

#include <variant>

namespace alia {

    struct pipeline_config {
        // Both alternatives are non-owning. The referenced effect/program must
        // outlive this pipeline.
        std::variant<const basic_effect *, shader_program *> effect = static_cast<const basic_effect *>(nullptr);
        blend_state blend{true, blend_factor::src_alpha, blend_factor::inv_src_alpha, blend_op::add};
        depth_state depth = {};
        raster_state raster = {};
    };

    class render_pass;

    class pipeline {
    public:
        pipeline() = delete;
        ~pipeline();
        pipeline(pipeline &&) noexcept;
        pipeline &operator=(pipeline &&) noexcept;
        pipeline(const pipeline &) = delete;
        pipeline &operator=(const pipeline &) = delete;

        template <vertex_type TVertex>
        [[nodiscard]] static pipeline create(gfx_device &device, const pipeline_config &config = {}) {
            return pipeline(device, config, detail::vertex_definition_of<TVertex>(), false);
        }

        [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
        [[nodiscard]] pipeline_handle *impl() const noexcept { return handle_; }
        [[nodiscard]] const graphics_backend_interface *backend() const noexcept { return backend_; }
        [[nodiscard]] device_handle *device() const noexcept { return device_; }
        [[nodiscard]] bool uses_shader() const noexcept { return desc_.shader != nullptr; }
        [[nodiscard]] bool depth_enabled() const noexcept { return desc_.depth.test_enabled || desc_.depth.write_enabled; }
        [[nodiscard]] std::size_t vertex_definition_index() const noexcept { return desc_.vertex_layout.index; }
        [[nodiscard]] bool is_dynamic() const noexcept { return dynamic_; }
        [[nodiscard]] const basic_effect *effect() const noexcept { return desc_.effect; }

    protected:
        pipeline(gfx_device &, const pipeline_config &, const vertex_definition_view &, bool dynamic);
        void update_desc();
        void resolve_effect(std::variant<const basic_effect *, shader_program *> effect);

        pipeline_handle *handle_ = nullptr;
        const graphics_backend_interface *backend_ = nullptr;
        device_handle *device_ = nullptr;
        pipeline_desc desc_ = {};
        bool dynamic_ = false;

        friend class render_pass;
    };

    class dynamic_pipeline : public pipeline {
    public:
        dynamic_pipeline() = delete;
        explicit dynamic_pipeline(gfx_device &device, const pipeline_config &config = {});

        template <vertex_type TVertex>
        void set_vertex_type() { set_vertex_definition(detail::vertex_definition_of<TVertex>()); }
        void set_effect(std::variant<const basic_effect *, shader_program *> effect);
        void set_blend(const blend_state &state);
        void set_depth(const depth_state &state);
        void set_raster(const raster_state &state);

    private:
        void set_vertex_definition(const vertex_definition_view &definition);
        friend class render_pass;
    };

} // namespace alia

#endif
