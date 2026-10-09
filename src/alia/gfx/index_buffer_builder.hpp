#ifndef ALIA_GFX_INDEX_BUFFER_BUILDER_HPP
#define ALIA_GFX_INDEX_BUFFER_BUILDER_HPP

#include "graphics_backend_interface.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace alia {

    // Collects indices as uint16_t. The first index above 0xFFFF moves the
    // contents to uint32_t; clear() returns to uint16_t and keeps both capacities.
    class index_buffer_builder {
    public:
        void add(uint32_t index) {
            if (!is_wide_ && index > 0xFFFF)
                widen();
            if (is_wide_)
                wide_.push_back(index);
            else
                narrow_.push_back(static_cast<uint16_t>(index));
        }

        void add(std::span<const uint32_t> indices) {
            if (!is_wide_ && std::ranges::any_of(indices, [](uint32_t index) { return index > 0xFFFF; }))
                widen();
            if (is_wide_) {
                wide_.insert(wide_.end(), indices.begin(), indices.end());
            } else {
                for (uint32_t index : indices)
                    narrow_.push_back(static_cast<uint16_t>(index));
            }
        }

        void clear() noexcept {
            narrow_.clear();
            wide_.clear();
            is_wide_ = false;
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return is_wide_ ? wide_.size() : narrow_.size();
        }

        [[nodiscard]] bool empty() const noexcept {
            return size() == 0;
        }

        [[nodiscard]] index_format format() const noexcept {
            return is_wide_ ? index_format::u32 : index_format::u16;
        }

        // Calls fn with the active const span; both widths must have the same return type.
        template <class F>
        decltype(auto) visit(F &&fn) const {
            if (is_wide_)
                return std::forward<F>(fn)(std::span<const uint32_t>(wide_));
            return std::forward<F>(fn)(std::span<const uint16_t>(narrow_));
        }

    private:
        void widen() {
            wide_.assign(narrow_.begin(), narrow_.end());
            narrow_.clear();
            is_wide_ = true;
        }

        std::vector<uint16_t> narrow_;
        std::vector<uint32_t> wide_;
        bool is_wide_ = false;
    };

} // namespace alia

#endif // ALIA_GFX_INDEX_BUFFER_BUILDER_HPP
