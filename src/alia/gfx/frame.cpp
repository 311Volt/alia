#include "frame.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace alia {
    namespace {
        [[nodiscard]] vec2i mip_size(vec2i size, int level) {
            return {(std::max)(1, size.x >> level), (std::max)(1, size.y >> level)};
        }
        void validate_range(const char *name, int first, int count, int total) {
            if (first < 0 || count < 0 || first > total || count > total - first)
                throw std::out_of_range(std::string(name) + ": range is outside the buffer");
        }
        bool same_transform(const transform &a, const transform &b) noexcept {
            for (int row = 0; row != 4; ++row)
                for (int column = 0; column != 4; ++column)
                    if (a.m[row][column] != b.m[row][column])
                        return false;
            return true;
        }
        bool same_transforms(const transform_state &a, const transform_state &b) noexcept {
            return same_transform(a.world, b.world) && same_transform(a.view, b.view) &&
                same_transform(a.projection, b.projection);
        }
        bool same_render_state(const render_state &a, const render_state &b) noexcept {
            return a.shader == b.shader && a.vertex_layout.index == b.vertex_layout.index &&
                a.vertex_layout.stride == b.vertex_layout.stride && a.texture_op == b.texture_op &&
                a.blend == b.blend && a.depth == b.depth && a.raster.cull == b.raster.cull &&
                a.lighting == b.lighting && a.vertex_color_material == b.vertex_color_material &&
                a.fog == b.fog;
        }
        bool is_finite(color value) noexcept {
            return std::isfinite(value.r) && std::isfinite(value.g) &&
                std::isfinite(value.b) && std::isfinite(value.a);
        }
        bool is_finite(vec3f value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
        void validate_direction(vec3f direction) {
            if (!is_finite(direction))
                throw std::invalid_argument("frame::set_lights: direction must be finite");
            if (direction == vec3f{})
                throw std::invalid_argument("frame::set_lights: direction must be nonzero");
        }
        void validate_attenuation(const light_attenuation &attenuation) {
            if (!std::isfinite(attenuation.constant) || !std::isfinite(attenuation.linear) ||
                !std::isfinite(attenuation.quadratic))
                throw std::invalid_argument("frame::set_lights: attenuation must be finite");
            if (attenuation.constant < 0.0f || attenuation.linear < 0.0f ||
                attenuation.quadratic < 0.0f ||
                (attenuation.constant == 0.0f && attenuation.linear == 0.0f && attenuation.quadratic == 0.0f))
                throw std::invalid_argument("frame::set_lights: attenuation must be nonnegative and not all zero");
        }
        void validate_light(const light &value) {
            if (value.valueless_by_exception())
                throw std::invalid_argument("frame::set_lights: light has no value");
            std::visit([](const auto &source) {
                using source_type = std::decay_t<decltype(source)>;
                if (!is_finite(source.diffuse) || !is_finite(source.specular))
                    throw std::invalid_argument("frame::set_lights: light colors must be finite");
                if constexpr (!std::is_same_v<source_type, point_light>)
                    validate_direction(source.direction);
                if constexpr (!std::is_same_v<source_type, directional_light>) {
                    if (!is_finite(source.position))
                        throw std::invalid_argument("frame::set_lights: position must be finite");
                    validate_attenuation(source.attenuation);
                }
                if constexpr (std::is_same_v<source_type, spot_light>) {
                    const auto &cone = source.inner_outer;
                    const auto &cutoff = source.cutoff_exponent;
                    if (!std::isfinite(cone.inner) || !std::isfinite(cone.outer) ||
                        !std::isfinite(cutoff.cutoff) || !std::isfinite(cutoff.exponent))
                        throw std::invalid_argument("frame::set_lights: spot parameters must be finite");
                    if (cone.inner < 0.0f || cone.outer < cone.inner || cone.outer > std::numbers::pi_v<float>)
                        throw std::invalid_argument("frame::set_lights: spot angles must satisfy 0 <= inner <= outer <= pi");
                    if (cutoff.cutoff < 0.0f || cutoff.cutoff > std::numbers::pi_v<float> * 0.5f ||
                        cutoff.exponent < 0.0f || cutoff.exponent > 128.0f)
                        throw std::invalid_argument("frame::set_lights: spot cutoff must be in [0, pi/2] and exponent in [0, 128]");
                }
            }, value);
        }
    }

    bool frame::stored_lighting::operator==(const stored_lighting &other) const noexcept {
        return ambient == other.ambient && count == other.count &&
            std::equal(lights.begin(), lights.begin() + count, other.lights.begin());
    }

    frame::state_scope::state_scope(frame &owner) noexcept
        : frame_(&owner), shader_(owner.shader_), blend_(owner.blend_),
          depth_(owner.depth_), cull_(owner.cull_), transforms_(owner.transforms_),
          lighting_(owner.lighting_), material_(owner.material_), fog_(owner.fog_) {}
    frame::state_scope::state_scope(state_scope &&other) noexcept
        : frame_(std::exchange(other.frame_, nullptr)), shader_(other.shader_),
          blend_(other.blend_), depth_(other.depth_), cull_(other.cull_),
          transforms_(other.transforms_), lighting_(other.lighting_), material_(other.material_), fog_(other.fog_) {}
    frame::state_scope::~state_scope() {
        if (!frame_ || !frame_->active_)
            return;
        if (frame_->shader_ != shader_ || frame_->blend_ != blend_ ||
            frame_->depth_ != depth_ || frame_->cull_ != cull_)
            frame_->render_state_dirty_ = true;
        if (!same_transforms(frame_->transforms_, transforms_))
            frame_->transforms_dirty_ = true;
        if (frame_->lighting_ != lighting_)
            frame_->lighting_dirty_ = true;
        if (frame_->material_ != material_)
            frame_->material_dirty_ = true;
        if (frame_->fog_ != fog_)
            frame_->fog_dirty_ = true;
        frame_->shader_ = shader_;
        frame_->blend_ = blend_;
        frame_->depth_ = depth_;
        frame_->cull_ = cull_;
        frame_->transforms_ = transforms_;
        frame_->lighting_ = lighting_;
        frame_->material_ = material_;
        frame_->fog_ = fog_;
    }

    frame::frame(swapchain &swapchain) : swapchain_(&swapchain) {
        if (!swapchain.handle_)
            throw std::logic_error("swapchain::begin_frame: swapchain is not valid");
        if (swapchain.frame_active_)
            throw std::logic_error("swapchain::begin_frame: a frame is already active");
        swapchain.backend_->swapchain_begin_frame.get_or_throw()(swapchain.handle_);
        swapchain.frame_active_ = true;
        active_ = true;
        try {
            set_target();
        } catch (...) {
            finish_without_present();
            throw;
        }
    }
    frame::~frame() { finish_without_present(); }
    frame::frame(frame &&other) noexcept
        : swapchain_(std::exchange(other.swapchain_, nullptr)), shader_(other.shader_),
          blend_(other.blend_), depth_(other.depth_), cull_(other.cull_),
          transforms_(other.transforms_), lighting_(other.lighting_), material_(other.material_), fog_(other.fog_),
          render_state_dirty_(true), transforms_dirty_(true),
          lighting_dirty_(true), material_dirty_(true), fog_dirty_(true),
          slot0_kind_(other.slot0_kind_),
          target_size_(other.target_size_), target_has_depth_(other.target_has_depth_),
          texture_bindings_(std::move(other.texture_bindings_)), active_(std::exchange(other.active_, false)) {}
    frame &frame::operator=(frame &&other) noexcept {
        if (this != &other) {
            finish_without_present();
            swapchain_ = std::exchange(other.swapchain_, nullptr);
            shader_ = other.shader_;
            blend_ = other.blend_;
            depth_ = other.depth_;
            cull_ = other.cull_;
            state_ = {};
            transforms_ = other.transforms_;
            applied_transforms_.reset();
            lighting_ = other.lighting_;
            material_ = other.material_;
            fog_ = other.fog_;
            applied_lighting_.reset();
            applied_material_.reset();
            applied_fog_.reset();
            render_state_dirty_ = true;
            transforms_dirty_ = true;
            lighting_dirty_ = true;
            material_dirty_ = true;
            fog_dirty_ = true;
            slot0_kind_ = other.slot0_kind_;
            target_size_ = other.target_size_;
            target_has_depth_ = other.target_has_depth_;
            texture_bindings_ = std::move(other.texture_bindings_);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }
    void frame::finish_without_present() noexcept {
        if (!active_)
            return;
        try {
            swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        } catch (...) {
            // Destructors must not throw. The swapchain is still made reusable.
        }
        swapchain_->frame_active_ = false;
        shader_ = nullptr;
        texture_bindings_.clear();
        active_ = false;
        swapchain_ = nullptr;
    }
    void frame::present() {
        ensure_active();
        swapchain_->backend_->swapchain_end_frame.get_or_throw()(swapchain_->handle_);
        swapchain_->backend_->swapchain_present.get_or_throw()(swapchain_->handle_);
        swapchain_->frame_active_ = false;
        shader_ = nullptr;
        texture_bindings_.clear();
        active_ = false;
        swapchain_ = nullptr;
    }
    void frame::present(rect_i region) {
        ensure_active();
        if (region.width() <= 0 || region.height() <= 0 ||
            !rect_i{{}, swapchain_->size_}.contains(region))
            throw std::invalid_argument("frame::present: region is outside the swapchain");
        auto *swapchain = swapchain_;
        swapchain->backend_->swapchain_end_frame.get_or_throw()(swapchain->handle_);
        bool presented = true;
        try {
            if (swapchain->props_.update_display_region)
                presented = swapchain->backend_->swapchain_present_region.get_or_throw()(swapchain->handle_, region);
            else
                swapchain->backend_->swapchain_present.get_or_throw()(swapchain->handle_);
        } catch (...) {
            swapchain->frame_active_ = false;
            shader_ = nullptr;
            texture_bindings_.clear();
            active_ = false;
            swapchain_ = nullptr;
            throw;
        }
        swapchain->frame_active_ = false;
        shader_ = nullptr;
        texture_bindings_.clear();
        active_ = false;
        swapchain_ = nullptr;
        if (!presented)
            throw std::runtime_error("frame::present: backend failed to present region");
    }
    frame swapchain::begin_frame() { return frame(*this); }

    void frame::ensure_active() const {
        if (!active_ || !swapchain_)
            throw std::logic_error("frame: frame is not active");
    }
    void frame::unbind_render_target_source(texture_handle *target) {
        if (!target)
            return;
        for (auto it = texture_bindings_.begin(); it != texture_bindings_.end();) {
            if (it->second != target) {
                ++it;
                continue;
            }
            swapchain_->backend_->bind_resources.get_or_throw()(swapchain_->device_, {it->first, nullptr, {}});
            if (it->first == 0)
                slot0_kind_ = slot0_kind::none;
            it = texture_bindings_.erase(it);
        }
    }
    void frame::set_target() {
        ensure_active();
        const render_target_info info{
            swapchain_->handle_, nullptr, 0, 0, swapchain_->size_};
        if (!swapchain_->backend_->set_render_target.get_or_throw()(swapchain_->device_, info))
            throw std::runtime_error("frame::set_target: backend failed to select the swapchain backbuffer");
        target_size_ = info.target_size;
        target_has_depth_ = swapchain_->has_depth();
        set_projection(ui_projection());
    }
    void frame::set_target(texture &target, int level) {
        ensure_active();
        if (target.backend() != swapchain_->backend_ || target.device() != swapchain_->device_)
            throw std::invalid_argument("frame::set_target: texture belongs to another gfx_device");
        if (target.usage() != texture_usage::render_target)
            throw std::invalid_argument("frame::set_target: texture is not a render target");
        if (level < 0 || level >= target.mip_levels())
            throw std::out_of_range("frame::set_target: texture mip level out of range");
        unbind_render_target_source(target.impl());
        const render_target_info info{
            nullptr, target.impl(), level, 0, mip_size(target.size(), level)};
        if (!swapchain_->backend_->set_render_target.get_or_throw()(swapchain_->device_, info))
            throw std::runtime_error("frame::set_target: render-to-texture may be unsupported on this device");
        target_size_ = info.target_size;
        target_has_depth_ = false;
        set_projection(ui_projection());
    }
    void frame::set_target(cube_texture &target, cube_face face, int level) {
        ensure_active();
        if (target.backend() != swapchain_->backend_ ||
            target.device() != swapchain_->device_)
            throw std::invalid_argument(
                "frame::set_target: cube texture belongs to another gfx_device");
        if (target.usage() != texture_usage::render_target)
            throw std::invalid_argument(
                "frame::set_target: cube texture is not a render target");
        if (static_cast<int>(face) < 0 ||
            static_cast<int>(face) >= cube_face_count)
            throw std::out_of_range("frame::set_target: cube face out of range");
        if (level < 0 || level >= target.mip_levels())
            throw std::out_of_range(
                "frame::set_target: cube texture mip level out of range");
        unbind_render_target_source(target.impl());
        const render_target_info info{
            nullptr,
            target.impl(),
            level,
            static_cast<int>(face),
            mip_size(target.face_size(), level)
        };
        if (!swapchain_->backend_->set_render_target.get_or_throw()(
                swapchain_->device_, info))
            throw std::runtime_error(
                "frame::set_target: render-to-cube-face may be unsupported on this device");
        target_size_ = info.target_size;
        target_has_depth_ = false;
        set_projection(ui_projection());
    }
    void frame::clear(std::optional<color> color, std::optional<float> depth) {
        ensure_active();
        if (depth && !target_has_depth_)
            throw std::invalid_argument("frame::clear: current render target has no depth attachment");
        if (!color && !depth)
            return;
        if (!swapchain_->backend_->clear.get_or_throw()(swapchain_->device_, color, depth))
            throw std::runtime_error("frame::clear: backend clear failed");
    }
    void frame::set_blend(const blend_state &blend) {
        ensure_active();
        if (blend_ != blend) {
            blend_ = blend;
            render_state_dirty_ = true;
        }
    }
    void frame::set_depth(const depth_state &depth) {
        ensure_active();
        if (depth_ != depth) {
            depth_ = depth;
            render_state_dirty_ = true;
        }
    }
    void frame::set_cull(cull_mode cull) {
        ensure_active();
        if (cull_ != cull) {
            cull_ = cull;
            render_state_dirty_ = true;
        }
    }
    void frame::set_shader(shader_program &shader) {
        ensure_active();
        if (!shader)
            throw std::invalid_argument("frame::set_shader: shader is not valid");
        if (shader.backend() != backend())
            throw std::invalid_argument("frame::set_shader: shader belongs to another gfx_device");
        if (shader_ != &shader) {
            shader_ = &shader;
            render_state_dirty_ = true;
        }
    }
    void frame::set_shader(std::nullptr_t) {
        ensure_active();
        if (shader_) {
            shader_ = nullptr;
            render_state_dirty_ = true;
        }
    }
    void frame::set_world(const transform &world) {
        ensure_active();
        if (!same_transform(transforms_.world, world)) {
            transforms_.world = world;
            transforms_dirty_ = true;
        }
    }
    void frame::set_view(const transform &view) {
        ensure_active();
        if (!same_transform(transforms_.view, view)) {
            transforms_.view = view;
            transforms_dirty_ = true;
        }
    }
    void frame::set_projection(const transform &projection) {
        ensure_active();
        if (!same_transform(transforms_.projection, projection)) {
            transforms_.projection = projection;
            transforms_dirty_ = true;
        }
    }
    transform frame::ui_projection() const {
        ensure_active();
        const vec2f offset = backend()->pixel_center_offset;
        return transform::ortho(
            -offset.x, target_size_.x - offset.x, target_size_.y - offset.y, -offset.y);
    }
    frame::state_scope frame::save_state() {
        ensure_active();
        return state_scope(*this);
    }
    frame::state_scope frame::push_world(const transform &local) {
        auto saved = save_state();
        set_world(local * world());
        return saved;
    }
    void frame::set_lights(std::span<const light> lights) {
        ensure_active();
        if (!lights.empty() && !backend()->set_lighting.is_supported())
            (void)backend()->set_lighting.get_or_throw();
        if (lights.size() > static_cast<std::size_t>((std::min)(max_lights, backend()->caps.max_lights)))
            throw std::invalid_argument("frame::set_lights: light count exceeds caps().max_lights");
        for (const auto &value : lights)
            validate_light(value);
        const int count = static_cast<int>(lights.size());
        if (lighting_.count != count || !std::equal(lights.begin(), lights.end(), lighting_.lights.begin())) {
            std::copy(lights.begin(), lights.end(), lighting_.lights.begin());
            lighting_.count = count;
            lighting_dirty_ = true;
        }
    }
    void frame::set_lights(std::initializer_list<light> lights) {
        set_lights(std::span<const light>(lights.begin(), lights.size()));
    }
    void frame::set_ambient(color ambient) {
        ensure_active();
        if (!is_finite(ambient))
            throw std::invalid_argument("frame::set_ambient: color must be finite");
        if (lighting_.ambient != ambient) {
            lighting_.ambient = ambient;
            lighting_dirty_ = true;
        }
    }
    void frame::set_material(const material &surface) {
        ensure_active();
        if (!is_finite(surface.diffuse) || !is_finite(surface.ambient) ||
            !is_finite(surface.specular) || !is_finite(surface.emissive) || !std::isfinite(surface.shininess))
            throw std::invalid_argument("frame::set_material: material components must be finite");
        if (surface.shininess < 0.0f || surface.shininess > 128.0f)
            throw std::invalid_argument("frame::set_material: shininess must be in [0, 128]");
        if (material_ != surface) {
            material_ = surface;
            material_dirty_ = true;
        }
    }
    void frame::set_fog(const fog_state &fog) {
        ensure_active();
        switch (fog.mode) {
        case fog_mode::none:
        case fog_mode::linear:
        case fog_mode::exp:
        case fog_mode::exp2:
            break;
        default:
            throw std::invalid_argument("frame::set_fog: invalid fog mode");
        }
        if (!is_finite(fog.col) || !std::isfinite(fog.start) ||
            !std::isfinite(fog.end) || !std::isfinite(fog.density))
            throw std::invalid_argument("frame::set_fog: fog components must be finite");
        if (fog.density < 0.0f)
            throw std::invalid_argument("frame::set_fog: density must be nonnegative");
        if (fog.mode == fog_mode::linear && fog.start == fog.end)
            throw std::invalid_argument("frame::set_fog: linear fog requires start != end");
        if (fog.mode != fog_mode::none && !backend()->set_fog.is_supported())
            (void)backend()->set_fog.get_or_throw();
        if (fog_ != fog) {
            fog_ = fog;
            fog_dirty_ = true;
        }
    }
    void frame::set_texture(int slot, texture &texture) { set_texture(slot, texture, texture.sampler()); }
    void frame::set_texture(int slot, texture &texture, const sampler_state &sampler) {
        ensure_active();
        if (slot < 0)
            throw std::invalid_argument("frame::set_texture: slot must be non-negative");
        if (texture.backend() != swapchain_->backend_ || texture.device() != swapchain_->device_)
            throw std::invalid_argument("frame::set_texture: texture belongs to another gfx_device");
        swapchain_->backend_->bind_resources.get_or_throw()(swapchain_->device_, {slot, texture.impl(), sampler});
        texture_bindings_[slot] = texture.impl();
        if (slot == 0)
            slot0_kind_ = texture.role() == texture_role::alpha_mask
                ? slot0_kind::alpha_mask : slot0_kind::color;
    }
    void frame::set_texture(int slot, cube_texture &texture) {
        set_texture(slot, texture, texture.sampler());
    }
    void frame::set_texture(
        int slot, cube_texture &texture, const sampler_state &sampler
    ) {
        ensure_active();
        if (slot < 0)
            throw std::invalid_argument("frame::set_texture: slot must be non-negative");
        if (texture.backend() != swapchain_->backend_ ||
            texture.device() != swapchain_->device_)
            throw std::invalid_argument(
                "frame::set_texture: cube texture belongs to another gfx_device");
        swapchain_->backend_->bind_resources.get_or_throw()(
            swapchain_->device_, {slot, texture.impl(), sampler});
        texture_bindings_[slot] = texture.impl();
        if (slot == 0)
            slot0_kind_ = slot0_kind::cube;
    }
    void frame::set_viewport(const render_viewport &viewport) {
        ensure_active();
        swapchain_->backend_->set_viewport.get_or_throw()(swapchain_->device_, viewport);
    }
    void frame::prepare_draw(const vertex_definition_view &definition) {
        ensure_active();
        if (definition.stride <= 0)
            throw std::invalid_argument("frame: vertex definition has no layout");
        const bool has_tex_coord = std::any_of(
            definition.elements.begin(), definition.elements.end(),
            [](const vertex_element &element) { return element.attribute == vertex_attr::tex_coord; });
        const bool has_normal = std::any_of(
            definition.elements.begin(), definition.elements.end(),
            [](const vertex_element &element) { return element.attribute == vertex_attr::normal; });
        const bool has_color = std::any_of(
            definition.elements.begin(), definition.elements.end(),
            [](const vertex_element &element) { return element.attribute == vertex_attr::color_attr; });
        const bool lit = lighting_.count != 0 && has_normal && !shader_;
        const auto texture_op = !has_tex_coord || slot0_kind_ == slot0_kind::none || slot0_kind_ == slot0_kind::cube
            ? texture_operation::vertex_color
            : slot0_kind_ == slot0_kind::alpha_mask ? texture_operation::alpha_mask : texture_operation::modulate;
        const render_state next{
            shader_ ? shader_->impl() : nullptr, definition, texture_op,
            blend_, depth_, {cull_}, lit, lit && material_.use_vertex_color && has_color,
            fog_.mode != fog_mode::none && !shader_};
        const bool render_state_changed = !same_render_state(state_, next);
        if (render_state_changed)
            render_state_dirty_ = true;
        if (!target_has_depth_ && (depth_.test_enabled || depth_.write_enabled))
            throw std::invalid_argument("frame::draw: depth state requires a depth attachment");
        if (render_state_dirty_) {
            if (render_state_changed) {
                backend()->set_render_state.get_or_throw()(device(), next);
                state_ = next;
            }
            render_state_dirty_ = false;
        }
        if (transforms_dirty_) {
            if (!applied_transforms_ || !same_transforms(*applied_transforms_, transforms_)) {
                backend()->set_transforms.get_or_throw()(device(), transforms_);
                applied_transforms_ = transforms_;
            }
            transforms_dirty_ = false;
        }
        if (lighting_dirty_ && backend()->set_lighting.is_supported()) {
            if (!applied_lighting_ || *applied_lighting_ != lighting_) {
                backend()->set_lighting.get_or_throw()(device(), {
                    lighting_.ambient, std::span<const light>(lighting_.lights.data(), lighting_.count)});
                applied_lighting_ = lighting_;
            }
            lighting_dirty_ = false;
        }
        if (material_dirty_ && backend()->set_material.is_supported()) {
            if (!applied_material_ || *applied_material_ != material_) {
                backend()->set_material.get_or_throw()(device(), material_);
                applied_material_ = material_;
            }
            material_dirty_ = false;
        }
        if (fog_dirty_ && backend()->set_fog.is_supported()) {
            if (!applied_fog_ || *applied_fog_ != fog_) {
                backend()->set_fog.get_or_throw()(device(), fog_);
                applied_fog_ = fog_;
            }
            fog_dirty_ = false;
        }
    }
    void frame::copy_to_texture(texture &dst, rect_i src_rect, vec2i dst_pos, int dst_level) {
        ensure_active();
        if (dst.backend() != swapchain_->backend_ || dst.device() != swapchain_->device_)
            throw std::invalid_argument("frame::copy_to_texture: texture belongs to another gfx_device");
        if (dst_level < 0 || dst_level >= dst.mip_levels())
            throw std::out_of_range("frame::copy_to_texture: destination mip level out of range");
        if (src_rect.width() <= 0 || src_rect.height() <= 0 || dst_pos.x < 0 || dst_pos.y < 0)
            throw std::invalid_argument("frame::copy_to_texture: invalid source rectangle or destination position");
        if (!rect_i{{}, target_size_}.contains(src_rect))
            throw std::invalid_argument("frame::copy_to_texture: source is outside the current target");
        if (!rect_i{{}, mip_size(dst.size(), dst_level)}.contains(rect_i::pos_size(dst_pos, src_rect.size())))
            throw std::invalid_argument("frame::copy_to_texture: destination is outside texture level");
        if (!swapchain_->backend_->copy_render_target_to_texture.get_or_throw()(swapchain_->device_, dst.impl(), src_rect, target_size_, dst_pos, dst_level, 0))
            throw std::runtime_error("frame::copy_to_texture: backend copy failed");
    }
    void frame::copy_to_texture(
        cube_texture &dst,
        cube_face face,
        rect_i src_rect,
        vec2i dst_pos,
        int dst_level
    ) {
        ensure_active();
        if (dst.backend() != swapchain_->backend_ || dst.device() != swapchain_->device_)
            throw std::invalid_argument(
                "frame::copy_to_texture: cube texture belongs to another gfx_device");
        if (static_cast<int>(face) < 0 || static_cast<int>(face) >= cube_face_count)
            throw std::out_of_range("frame::copy_to_texture: cube face out of range");
        if (dst_level < 0 || dst_level >= dst.mip_levels())
            throw std::out_of_range(
                "frame::copy_to_texture: destination mip level out of range");
        if (src_rect.width() <= 0 || src_rect.height() <= 0 ||
            dst_pos.x < 0 || dst_pos.y < 0)
            throw std::invalid_argument(
                "frame::copy_to_texture: invalid source rectangle or destination position");
        if (!rect_i{{}, target_size_}.contains(src_rect))
            throw std::invalid_argument(
                "frame::copy_to_texture: source is outside the current target");
        if (!rect_i{{}, mip_size(dst.face_size(), dst_level)}.contains(
                rect_i::pos_size(dst_pos, src_rect.size())))
            throw std::invalid_argument(
                "frame::copy_to_texture: destination is outside cube texture level");
        if (!swapchain_->backend_->copy_render_target_to_texture.get_or_throw()(
                swapchain_->device_, dst.impl(), src_rect, target_size_, dst_pos,
                dst_level, static_cast<int>(face)))
            throw std::runtime_error("frame::copy_to_texture: backend copy failed");
    }

    namespace detail {
        void frame_draw_transient(frame &frame, const void *vertices, int vertex_count, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw: vertex data is null");
            frame.prepare_draw(definition);
            frame.backend()->upload_transient_vertex_data.get_or_throw()(frame.device(), vertices, vertex_count * definition.stride);
            frame.backend()->draw.get_or_throw()(frame.device(), topology, vertex_count, 0);
        }
        void frame_draw_buffered(frame &frame, vertex_buffer_handle *vertices, int vertex_count, int first_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0)
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw: vertex buffer is not valid");
            const int total = frame.backend()->vertex_buffer_count.get_or_throw()(vertices);
            validate_range("frame::draw", first_vertex, vertex_count, total);
            frame.prepare_draw(definition);
            frame.backend()->bind_vertex_buffer.get_or_throw()(frame.device(), vertices);
            frame.backend()->draw.get_or_throw()(frame.device(), topology, vertex_count, first_vertex);
        }
        void frame_draw_indexed_transient(frame &frame, const void *vertices, int vertex_count, std::span<const uint32_t> indices, const vertex_definition_view &definition, primitive_topology topology) {
            if (vertex_count <= 0 || indices.empty())
                return;
            if (!vertices)
                throw std::invalid_argument("frame::draw_indexed: vertex data is null");
            frame.prepare_draw(definition);
            frame.backend()->upload_transient_vertex_data.get_or_throw()(frame.device(), vertices, vertex_count * definition.stride);
            frame.backend()->upload_transient_index_data.get_or_throw()(frame.device(), indices);
            frame.backend()->draw_indexed.get_or_throw()(frame.device(), topology, static_cast<int>(indices.size()), 0, 0);
        }
        void frame_draw_indexed_buffered(frame &frame, vertex_buffer_handle *vertices, int vertex_count, index_buffer_handle *indices, int index_total, int first_index, int index_count, int base_vertex, const vertex_definition_view &definition, primitive_topology topology) {
            if (index_count < 0)
                index_count = index_total - first_index;
            if (vertex_count <= 0 || index_count <= 0)
                return;
            if (!vertices || !indices)
                throw std::invalid_argument("frame::draw_indexed: buffer is not valid");
            validate_range("frame::draw_indexed", first_index, index_count, index_total);
            if (base_vertex < 0 || base_vertex >= vertex_count)
                throw std::out_of_range("frame::draw_indexed: base vertex is outside the vertex buffer");
            frame.prepare_draw(definition);
            frame.backend()->bind_vertex_buffer.get_or_throw()(frame.device(), vertices);
            frame.backend()->bind_index_buffer.get_or_throw()(frame.device(), indices);
            frame.backend()->draw_indexed.get_or_throw()(frame.device(), topology, index_count, first_index, base_vertex);
        }
    } // namespace detail
} // namespace alia
