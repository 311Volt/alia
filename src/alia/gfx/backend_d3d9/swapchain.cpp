#ifdef ALIA_COMPILE_GFX_BACKEND_D3D9

#include "d3d9_ops.hpp"

#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace alia {
    namespace {
        framebuffer_properties color_properties(D3DFORMAT format) {
            framebuffer_properties props;
            switch (format) {
            case D3DFMT_A8R8G8B8:
                props.color_format = pixel_format::bgra8888;
                props.color_bits = 32;
                props.red_bits = props.green_bits = props.blue_bits = props.alpha_bits = 8;
                props.red_shift = 16; props.green_shift = 8; props.blue_shift = 0; props.alpha_shift = 24;
                break;
            case D3DFMT_X8R8G8B8:
                props.color_format = pixel_format::bgra8888;
                props.color_bits = 32;
                props.red_bits = props.green_bits = props.blue_bits = 8;
                props.alpha_bits = 0;
                props.red_shift = 16; props.green_shift = 8; props.blue_shift = 0; props.alpha_shift = 24;
                break;
            case D3DFMT_R8G8B8:
                props.color_format = pixel_format::bgr888;
                props.color_bits = 24;
                props.red_bits = props.green_bits = props.blue_bits = 8;
                props.alpha_bits = 0;
                props.red_shift = 16; props.green_shift = 8; props.blue_shift = 0; props.alpha_shift = 0;
                break;
            case D3DFMT_A8B8G8R8:
            case D3DFMT_X8B8G8R8:
                props.color_format = pixel_format::rgba8888;
                props.color_bits = 32;
                props.red_bits = props.green_bits = props.blue_bits = 8;
                props.alpha_bits = format == D3DFMT_A8B8G8R8 ? 8 : 0;
                props.red_shift = 0; props.green_shift = 8; props.blue_shift = 16; props.alpha_shift = 24;
                break;
            case D3DFMT_R5G6B5:
                props.color_format = pixel_format::rgb565;
                props.color_bits = 16;
                props.red_bits = 5; props.green_bits = 6; props.blue_bits = 5; props.alpha_bits = 0;
                props.red_shift = 11; props.green_shift = 5; props.blue_shift = 0; props.alpha_shift = 0;
                break;
            case D3DFMT_X1R5G5B5:
                props.color_format = pixel_format::rgb565;
                props.color_bits = 16;
                props.red_bits = props.green_bits = props.blue_bits = 5; props.alpha_bits = 0;
                props.red_shift = 10; props.green_shift = 5; props.blue_shift = 0; props.alpha_shift = 15;
                break;
            case D3DFMT_A1R5G5B5:
                props.color_format = pixel_format::rgb565;
                props.color_bits = 16;
                props.red_bits = props.green_bits = props.blue_bits = 5; props.alpha_bits = 1;
                props.red_shift = 10; props.green_shift = 5; props.blue_shift = 0; props.alpha_shift = 15;
                break;
            case D3DFMT_A2R10G10B10:
                props.color_format = pixel_format::bgra8888;
                props.color_bits = 32;
                props.red_bits = props.green_bits = props.blue_bits = 10; props.alpha_bits = 2;
                props.red_shift = 20; props.green_shift = 10; props.blue_shift = 0; props.alpha_shift = 30;
                break;
            case D3DFMT_A2B10G10R10:
                props.color_format = pixel_format::rgba8888;
                props.color_bits = 32;
                props.red_bits = props.green_bits = props.blue_bits = 10; props.alpha_bits = 2;
                props.red_shift = 0; props.green_shift = 10; props.blue_shift = 20; props.alpha_shift = 30;
                break;
            default:
                break;
            }
            return props;
        }

        long long option_score(const gfx_option<int> &option, int actual) {
            return option.requested()
                ? static_cast<long long>(std::abs(option.value - actual))
                : 0;
        }

        long long color_score(
            const framebuffer_config &requested,
            const framebuffer_properties &actual) {
            return option_score(requested.color_bits, actual.color_bits) +
                   option_score(requested.red_bits, actual.red_bits) +
                   option_score(requested.green_bits, actual.green_bits) +
                   option_score(requested.blue_bits, actual.blue_bits) +
                   option_score(requested.alpha_bits, actual.alpha_bits) +
                   option_score(requested.red_shift, actual.red_shift) +
                   option_score(requested.green_shift, actual.green_shift) +
                   option_score(requested.blue_shift, actual.blue_shift) +
                   option_score(requested.alpha_shift, actual.alpha_shift);
        }

        template <class T>
        bool required_matches(const gfx_option<T> &option, const T &actual) {
            return !option.required() || option.value == actual;
        }

        bool color_requirements_match(
            const framebuffer_config &requested,
            const framebuffer_properties &actual) {
            return required_matches(requested.color_bits, actual.color_bits) &&
                required_matches(requested.red_bits, actual.red_bits) &&
                required_matches(requested.green_bits, actual.green_bits) &&
                required_matches(requested.blue_bits, actual.blue_bits) &&
                required_matches(requested.alpha_bits, actual.alpha_bits) &&
                required_matches(requested.red_shift, actual.red_shift) &&
                required_matches(requested.green_shift, actual.green_shift) &&
                required_matches(requested.blue_shift, actual.blue_shift) &&
                required_matches(requested.alpha_shift, actual.alpha_shift) &&
                required_matches(requested.float_color, false);
        }

        bool color_requested(const framebuffer_config &config) {
            return config.color_bits.requested() || config.red_bits.requested() ||
                   config.green_bits.requested() || config.blue_bits.requested() ||
                   config.alpha_bits.requested() || config.red_shift.requested() ||
                   config.green_shift.requested() || config.blue_shift.requested() ||
                   config.alpha_shift.requested() || config.float_color.requested();
        }

        D3DFORMAT choose_backbuffer_format(
            d3d9_device &device,
            const framebuffer_config &config,
            D3DFORMAT adapter_format) {
            if (!color_requested(config))
                return D3DFMT_UNKNOWN;

            static constexpr std::array formats{
                D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5,
                D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A2R10G10B10,
            };
            D3DFORMAT best = D3DFMT_UNKNOWN;
            long long best_score = (std::numeric_limits<long long>::max)();
            bool exact_requirements_available = false;
            for (D3DFORMAT candidate : formats) {
                if (SUCCEEDED(device.d3d->CheckDeviceType(
                        device.adapter, device.device_type,
                        adapter_format, candidate, TRUE)) &&
                    color_requirements_match(config, color_properties(candidate))) {
                    exact_requirements_available = true;
                    break;
                }
            }
            for (D3DFORMAT candidate : formats) {
                if (FAILED(device.d3d->CheckDeviceType(
                        device.adapter, device.device_type,
                        adapter_format, candidate, TRUE)))
                    continue;
                const auto props = color_properties(candidate);
                if (exact_requirements_available &&
                    !color_requirements_match(config, props))
                    continue;
                long long score = color_score(config, props);
                if (config.float_color.requested() && config.float_color.value)
                    score += 1000;
                if (score < best_score) {
                    best = candidate;
                    best_score = score;
                }
            }
            return best;
        }

        struct depth_format_info {
            D3DFORMAT format;
            int depth_bits;
            int stencil_bits;
            bool floating;
        };

        bool depth_format_supported(
            d3d9_device &device,
            D3DFORMAT adapter_format,
            D3DFORMAT backbuffer_format,
            D3DFORMAT candidate) {
            return SUCCEEDED(device.d3d->CheckDeviceFormat(
                       device.adapter, device.device_type, adapter_format,
                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, candidate)) &&
                   SUCCEEDED(device.d3d->CheckDepthStencilMatch(
                       device.adapter, device.device_type, adapter_format,
                       backbuffer_format, candidate));
        }

        D3DFORMAT choose_depth_format(
            d3d9_device &device,
            const framebuffer_config &config,
            D3DFORMAT adapter_format,
            D3DFORMAT backbuffer_format) {
            if (config.depth_bits.requested() && config.depth_bits.value == 0 &&
                (config.depth_bits.required() ||
                 (!config.stencil_bits.required() &&
                  !(config.float_depth.required() && config.float_depth.value))))
                return D3DFMT_UNKNOWN;

            static constexpr std::array formats{
                depth_format_info{D3DFMT_D24S8, 24, 8, false},
                depth_format_info{D3DFMT_D24X8, 24, 0, false},
                depth_format_info{D3DFMT_D24X4S4, 24, 4, false},
                depth_format_info{D3DFMT_D24FS8, 24, 8, true},
                depth_format_info{D3DFMT_D32, 32, 0, false},
                depth_format_info{D3DFMT_D16, 16, 0, false},
                depth_format_info{D3DFMT_D15S1, 15, 1, false},
            };

            const bool requested = config.depth_bits.requested() ||
                config.stencil_bits.requested() || config.float_depth.requested();
            if (!requested) {
                for (D3DFORMAT preferred : {D3DFMT_D24S8, D3DFMT_D16}) {
                    if (depth_format_supported(
                            device, adapter_format, backbuffer_format, preferred))
                        return preferred;
                }
                return D3DFMT_UNKNOWN;
            }

            D3DFORMAT best = D3DFMT_UNKNOWN;
            long long best_score = (std::numeric_limits<long long>::max)();
            bool exact_requirements_available = false;
            for (const auto &candidate : formats) {
                if (!depth_format_supported(
                        device, adapter_format, backbuffer_format, candidate.format))
                    continue;
                const bool matches =
                    required_matches(config.depth_bits, candidate.depth_bits) &&
                    required_matches(config.stencil_bits, candidate.stencil_bits) &&
                    required_matches(config.float_depth, candidate.floating);
                if (matches) {
                    exact_requirements_available = true;
                    break;
                }
            }
            for (const auto &candidate : formats) {
                if (!depth_format_supported(
                        device, adapter_format, backbuffer_format, candidate.format))
                    continue;
                if (exact_requirements_available &&
                    !(required_matches(config.depth_bits, candidate.depth_bits) &&
                      required_matches(config.stencil_bits, candidate.stencil_bits) &&
                      required_matches(config.float_depth, candidate.floating)))
                    continue;
                long long score = option_score(config.depth_bits, candidate.depth_bits) +
                                  option_score(config.stencil_bits, candidate.stencil_bits);
                if (config.float_depth.requested() &&
                    config.float_depth.value != candidate.floating)
                    ++score;
                if (score < best_score) {
                    best = candidate.format;
                    best_score = score;
                }
            }
            return best;
        }

        std::pair<int, int> depth_and_stencil_bits(D3DFORMAT format) {
            switch (format) {
            case D3DFMT_D24S8: return {24, 8};
            case D3DFMT_D24X8: return {24, 0};
            case D3DFMT_D24X4S4: return {24, 4};
            case D3DFMT_D24FS8: return {24, 8};
            case D3DFMT_D32: return {32, 0};
            case D3DFMT_D16: return {16, 0};
            case D3DFMT_D15S1: return {15, 1};
            default: return {0, 0};
            }
        }

        UINT presentation_interval(vsync_mode mode) {
            switch (mode) {
            case vsync_mode::disable: return D3DPRESENT_INTERVAL_IMMEDIATE;
            case vsync_mode::suggest: return D3DPRESENT_INTERVAL_DEFAULT;
            case vsync_mode::require: return D3DPRESENT_INTERVAL_ONE;
            }
            return D3DPRESENT_INTERVAL_IMMEDIATE;
        }

        bool supports_msaa(
            d3d9_device &device,
            D3DFORMAT color,
            D3DFORMAT depth,
            int samples,
            DWORD &quality) {
            if (samples < 2 || samples > 16)
                return false;
            const auto type = static_cast<D3DMULTISAMPLE_TYPE>(samples);
            DWORD color_levels = 0;
            if (FAILED(device.d3d->CheckDeviceMultiSampleType(
                    device.adapter, device.device_type, color, TRUE, type, &color_levels)))
                return false;
            DWORD depth_levels = color_levels;
            if (depth != D3DFMT_UNKNOWN && FAILED(device.d3d->CheckDeviceMultiSampleType(
                    device.adapter, device.device_type, depth, TRUE, type, &depth_levels)))
                return false;
            quality = 0;
            return color_levels > 0 && depth_levels > 0;
        }

        void select_presentation(
            d3d9_swapchain &swapchain,
            const swapchain_desc &desc,
            D3DFORMAT actual_color) {
            const auto &config = desc.framebuffer;
            const bool update_requested = config.update_display_region.requested() &&
                config.update_display_region.value;
            const bool copy_requested = config.swap.requested() &&
                config.swap.value == swap_method::copy;
            const bool flip_requested = config.swap.requested() &&
                config.swap.value == swap_method::flip;
            const bool non_discard_requested = update_requested || copy_requested || flip_requested;
            const bool non_discard_required =
                (config.update_display_region.required() && config.update_display_region.value) ||
                (config.swap.required() &&
                    (config.swap.value == swap_method::copy || config.swap.value == swap_method::flip));
            const bool msaa_requested = config.samples.requested() && config.samples.value > 0;

            if (config.update_display_region.required() && config.swap.required()) {
                const bool update_from_swap = config.swap.value == swap_method::copy;
                if (config.update_display_region.value != update_from_swap)
                    throw std::runtime_error(
                        "D3D9 required swap and update_display_region options conflict");
            }

            if (msaa_requested && non_discard_requested &&
                config.samples.required() && non_discard_required)
                throw std::runtime_error(
                    "D3D9 samples require discard swap and conflict with required copy/flip/update_display_region");

            const bool keep_non_discard = non_discard_required && !config.samples.required();
            const bool try_msaa = msaa_requested && !keep_non_discard;
            if (try_msaa) {
                const int minimum = config.samples.required() ? config.samples.value : 2;
                for (int samples = config.samples.value; samples >= minimum; --samples) {
                    DWORD quality = 0;
                    if (supports_msaa(
                            *swapchain.owner, actual_color, swapchain.depth_fmt,
                            samples, quality)) {
                        swapchain.msaa = static_cast<D3DMULTISAMPLE_TYPE>(samples);
                        swapchain.msaa_quality = quality;
                        break;
                    }
                }
            }

            if (swapchain.msaa != D3DMULTISAMPLE_NONE) {
                swapchain.swap = D3DSWAPEFFECT_DISCARD;
            } else if (config.swap.required()) {
                swapchain.swap = config.swap.value == swap_method::copy
                    ? D3DSWAPEFFECT_COPY
                    : config.swap.value == swap_method::flip
                        ? D3DSWAPEFFECT_FLIP : D3DSWAPEFFECT_DISCARD;
            } else if (config.update_display_region.required()) {
                if (config.update_display_region.value)
                    swapchain.swap = D3DSWAPEFFECT_COPY;
                else if (flip_requested)
                    swapchain.swap = D3DSWAPEFFECT_FLIP;
                else
                    swapchain.swap = D3DSWAPEFFECT_DISCARD;
            } else if (update_requested || copy_requested) {
                swapchain.swap = D3DSWAPEFFECT_COPY;
            } else if (flip_requested) {
                swapchain.swap = D3DSWAPEFFECT_FLIP;
            } else {
                swapchain.swap = D3DSWAPEFFECT_DISCARD;
            }
            swapchain.present_interval = presentation_interval(desc.vsync);
        }

        bool create_depth_stencil(d3d9_swapchain &swapchain) {
            if (swapchain.depth_fmt == D3DFMT_UNKNOWN)
                return true;
            return SUCCEEDED(swapchain.device->CreateDepthStencilSurface(
                static_cast<UINT>(swapchain.size.x), static_cast<UINT>(swapchain.size.y),
                swapchain.depth_fmt, swapchain.msaa, swapchain.msaa_quality,
                TRUE, &swapchain.depth_stencil, nullptr));
        }

        bool create_native_swapchain(d3d9_swapchain &swapchain) {
            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE;
            pp.SwapEffect = swapchain.swap;
            pp.BackBufferCount = swapchain.swap == D3DSWAPEFFECT_FLIP ? 2 : 1;
            pp.BackBufferFormat = swapchain.backbuffer_fmt;
            pp.BackBufferWidth = static_cast<UINT>(swapchain.size.x);
            pp.BackBufferHeight = static_cast<UINT>(swapchain.size.y);
            pp.MultiSampleType = swapchain.msaa;
            pp.MultiSampleQuality = swapchain.msaa_quality;
            pp.hDeviceWindow = swapchain.hwnd;
            pp.PresentationInterval = swapchain.present_interval;
            return SUCCEEDED(swapchain.device->CreateAdditionalSwapChain(
                &pp, &swapchain.swap_chain));
        }

        void update_properties(d3d9_swapchain &swapchain) {
            D3DPRESENT_PARAMETERS params = {};
            swapchain.swap_chain->GetPresentParameters(&params);
            IDirect3DSurface9 *backbuffer = nullptr;
            D3DSURFACE_DESC desc = {};
            if (SUCCEEDED(swapchain.swap_chain->GetBackBuffer(
                    0, D3DBACKBUFFER_TYPE_MONO, &backbuffer))) {
                backbuffer->GetDesc(&desc);
                backbuffer->Release();
            }
            swapchain.props = color_properties(desc.Format);
            const auto [depth, stencil] = depth_and_stencil_bits(swapchain.depth_fmt);
            swapchain.props.depth_bits = depth;
            swapchain.props.stencil_bits = stencil;
            swapchain.props.float_depth = swapchain.depth_fmt == D3DFMT_D24FS8;
            swapchain.props.sample_buffers = params.MultiSampleType == D3DMULTISAMPLE_NONE ? 0 : 1;
            swapchain.props.samples = swapchain.props.sample_buffers
                ? static_cast<int>(params.MultiSampleType) : 0;
            swapchain.props.swap = params.SwapEffect == D3DSWAPEFFECT_COPY
                ? swap_method::copy
                : params.SwapEffect == D3DSWAPEFFECT_FLIP
                    ? swap_method::flip : swap_method::undefined;
            swapchain.props.update_display_region = params.SwapEffect == D3DSWAPEFFECT_COPY;
            swapchain.props.vsync = params.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE;
        }
    }

    swapchain_handle *d3d9_create_swapchain(
        device_handle *dev_h,
        void *native_handle,
        vec2i size,
        const swapchain_desc &desc) {
        auto *dev = as_d3d9_device(dev_h);
        D3DDISPLAYMODE adapter_mode = {};
        if (FAILED(dev->d3d->GetAdapterDisplayMode(dev->adapter, &adapter_mode)))
            return nullptr;

        auto *swapchain = new d3d9_swapchain;
        swapchain->owner = dev;
        swapchain->device = dev->device;
        swapchain->hwnd = static_cast<HWND>(native_handle);
        swapchain->size = size;
        swapchain->backbuffer_fmt = choose_backbuffer_format(
            *dev, desc.framebuffer, adapter_mode.Format);
        const D3DFORMAT actual_color = swapchain->backbuffer_fmt == D3DFMT_UNKNOWN
            ? adapter_mode.Format : swapchain->backbuffer_fmt;
        swapchain->depth_fmt = choose_depth_format(
            *dev, desc.framebuffer, adapter_mode.Format, actual_color);
        try {
            select_presentation(*swapchain, desc, actual_color);
        } catch (...) {
            delete swapchain;
            throw;
        }

        if (!create_native_swapchain(*swapchain) || !create_depth_stencil(*swapchain)) {
            if (swapchain->depth_stencil) swapchain->depth_stencil->Release();
            if (swapchain->swap_chain) swapchain->swap_chain->Release();
            delete swapchain;
            return nullptr;
        }
        update_properties(*swapchain);
        return swapchain;
    }

    void d3d9_destroy_swapchain(swapchain_handle *h) {
        auto *swapchain = as_d3d9_swapchain(h);
        if (swapchain->depth_stencil) swapchain->depth_stencil->Release();
        if (swapchain->swap_chain) swapchain->swap_chain->Release();
        delete swapchain;
    }

    framebuffer_properties d3d9_swapchain_properties(const swapchain_handle *h) {
        return as_d3d9_swapchain(h)->props;
    }

    void d3d9_swapchain_begin_frame(swapchain_handle *h) {
        as_d3d9_swapchain(h)->device->BeginScene();
    }

    void d3d9_swapchain_end_frame(swapchain_handle *h) {
        auto *swapchain = as_d3d9_swapchain(h);
        d3d9_reset_frame_state(*swapchain->owner);
        swapchain->device->EndScene();
    }

    void d3d9_swapchain_present(swapchain_handle *h) {
        as_d3d9_swapchain(h)->swap_chain->Present(
            nullptr, nullptr, nullptr, nullptr, 0);
    }

    bool d3d9_swapchain_present_region(swapchain_handle *h, rect_i region) {
        auto *swapchain = as_d3d9_swapchain(h);
        if (swapchain->swap != D3DSWAPEFFECT_COPY)
            return false;
        const RECT rect{region.left(), region.top(), region.right(), region.bottom()};
        return SUCCEEDED(swapchain->swap_chain->Present(
            &rect, &rect, nullptr, nullptr, 0));
    }

    void d3d9_swapchain_on_resize(swapchain_handle *h, vec2i new_size) {
        auto *swapchain = as_d3d9_swapchain(h);
        swapchain->device->SetDepthStencilSurface(nullptr);
        if (swapchain->depth_stencil) {
            swapchain->depth_stencil->Release();
            swapchain->depth_stencil = nullptr;
        }
        if (swapchain->swap_chain) {
            swapchain->swap_chain->Release();
            swapchain->swap_chain = nullptr;
        }
        swapchain->size = new_size;
        if (!create_native_swapchain(*swapchain) || !create_depth_stencil(*swapchain))
            throw std::runtime_error("D3D9 failed to resize swapchain");
        update_properties(*swapchain);
    }
} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_D3D9
