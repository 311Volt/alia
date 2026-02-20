#ifndef CSTRING_VIEW_A4453D98_D8EF_423F_A7A4_5F105FE4D972
#define CSTRING_VIEW_A4453D98_D8EF_423F_A7A4_5F105FE4D972

#include <string>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <ostream>

template<class CharT, class Traits = std::char_traits<CharT>>
class basic_cstring_view {
public:
    // Member types
    using traits_type = Traits;
    using value_type = CharT;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using reference = CharT&;
    using const_reference = const CharT&;
    using const_iterator = const CharT*;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    static constexpr size_type npos = size_type(-1);

private:
    const_pointer data_;
    size_type size_;

public:
    // Constructors
    constexpr basic_cstring_view() noexcept : data_(nullptr), size_(0) {}
    
    constexpr basic_cstring_view(const basic_cstring_view&) noexcept = default;
    
    constexpr basic_cstring_view(const_pointer s)
        : data_(s), size_(s ? Traits::length(s) : 0) {}
    
	// pointer+size - calling this on a non-null-terminated range is invalid
    constexpr basic_cstring_view(const_pointer s, size_type count)
        : data_(s), size_(count) {
        // Verify null termination
        if (s && (count == 0 || Traits::eq(s[count], CharT{}))) {
            // Valid
        } else if (s) {
            throw std::invalid_argument("String must be null-terminated at specified position");
        }
    }

    // Assignment
    constexpr basic_cstring_view& operator=(const basic_cstring_view&) noexcept = default;

    // Iterator access using deducing this (C++23)
    constexpr auto begin(this auto&& self) noexcept {
        return self.data_;
    }

    constexpr auto end(this auto&& self) noexcept {
        return self.data_ + self.size_;
    }

    constexpr auto cbegin() const noexcept {
        return data_;
    }

    constexpr auto cend() const noexcept {
        return data_ + size_;
    }

    constexpr auto rbegin(this auto&& self) noexcept {
        return std::make_reverse_iterator(self.end());
    }

    constexpr auto rend(this auto&& self) noexcept {
        return std::make_reverse_iterator(self.begin());
    }

    constexpr auto crbegin() const noexcept {
        return std::make_reverse_iterator(cend());
    }

    constexpr auto crend() const noexcept {
        return std::make_reverse_iterator(cbegin());
    }

    // Element access using deducing this
    constexpr auto operator[](this auto&& self, size_type pos) noexcept -> decltype(auto) {
        return self.data_[pos];
    }

    constexpr auto at(this auto&& self, size_type pos) -> decltype(auto) {
        if (pos >= self.size_) {
            throw std::out_of_range("basic_cstring_view::at");
        }
        return self.data_[pos];
    }

    constexpr auto front(this auto&& self) noexcept -> decltype(auto) {
        return self.data_[0];
    }

    constexpr auto back(this auto&& self) noexcept -> decltype(auto) {
        return self.data_[self.size_ - 1];
    }

    constexpr auto data(this auto&& self) noexcept {
        return self.data_;
    }

    constexpr auto c_str(this auto&& self) noexcept {
        return self.data_;
    }

    // Capacity
    constexpr size_type size() const noexcept {
        return size_;
    }

    constexpr size_type length() const noexcept {
        return size_;
    }

    constexpr size_type max_size() const noexcept {
        return (npos - 1) / sizeof(CharT);
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return size_ == 0;
    }

    // Modifiers
    constexpr void remove_prefix(size_type n) {
        data_ += n;
        size_ -= n;
    }

    constexpr void remove_suffix(size_type n) {
        size_ -= n;
    }

    constexpr void swap(basic_cstring_view& v) noexcept {
        std::swap(data_, v.data_);
        std::swap(size_, v.size_);
    }

    // String operations
    constexpr size_type copy(CharT* dest, size_type count, size_type pos = 0) const {
        if (pos > size_) {
            throw std::out_of_range("basic_cstring_view::copy");
        }
        size_type rcount = std::min(count, size_ - pos);
        Traits::copy(dest, data_ + pos, rcount);
        return rcount;
    }

    constexpr basic_cstring_view substr(size_type pos = 0, size_type count = npos) const {
        if (pos > size_) {
            throw std::out_of_range("basic_cstring_view::substr");
        }
        size_type rcount = std::min(count, size_ - pos);
        return basic_cstring_view(data_ + pos, rcount);
    }

    // Create substring as std::string
    constexpr std::basic_string<CharT, Traits> create_substr(size_type first, size_type last) const {
        if (first > size_ || last > size_ || first > last) {
            throw std::out_of_range("basic_cstring_view::create_substr");
        }
        return std::basic_string<CharT, Traits>(data_ + first, last - first);
    }

    // Comparison
    constexpr int compare(basic_cstring_view v) const noexcept {
        size_type rlen = std::min(size_, v.size_);
        int ret = Traits::compare(data_, v.data_, rlen);
        if (ret == 0) {
            ret = (size_ == v.size_) ? 0 : (size_ < v.size_ ? -1 : 1);
        }
        return ret;
    }

    constexpr int compare(size_type pos1, size_type count1, basic_cstring_view v) const {
        return substr(pos1, count1).compare(v);
    }

    constexpr int compare(size_type pos1, size_type count1, basic_cstring_view v,
                         size_type pos2, size_type count2) const {
        return substr(pos1, count1).compare(v.substr(pos2, count2));
    }

    constexpr int compare(const_pointer s) const {
        return compare(basic_cstring_view(s));
    }

    constexpr int compare(size_type pos1, size_type count1, const_pointer s) const {
        return substr(pos1, count1).compare(basic_cstring_view(s));
    }

    constexpr int compare(size_type pos1, size_type count1,
                         const_pointer s, size_type count2) const {
        return substr(pos1, count1).compare(basic_cstring_view(s, count2));
    }

    // Starts with / ends with
    constexpr bool starts_with(basic_cstring_view sv) const noexcept {
        return size_ >= sv.size_ && Traits::compare(data_, sv.data_, sv.size_) == 0;
    }

    constexpr bool starts_with(CharT ch) const noexcept {
        return !empty() && Traits::eq(front(), ch);
    }

    constexpr bool starts_with(const_pointer s) const {
        return starts_with(basic_cstring_view(s));
    }

    constexpr bool ends_with(basic_cstring_view sv) const noexcept {
        return size_ >= sv.size_ && 
               Traits::compare(data_ + size_ - sv.size_, sv.data_, sv.size_) == 0;
    }

    constexpr bool ends_with(CharT ch) const noexcept {
        return !empty() && Traits::eq(back(), ch);
    }

    constexpr bool ends_with(const_pointer s) const {
        return ends_with(basic_cstring_view(s));
    }

    // Contains (C++23)
    constexpr bool contains(basic_cstring_view sv) const noexcept {
        return find(sv) != npos;
    }

    constexpr bool contains(CharT ch) const noexcept {
        return find(ch) != npos;
    }

    constexpr bool contains(const_pointer s) const {
        return find(s) != npos;
    }

    // Find operations
    constexpr size_type find(basic_cstring_view v, size_type pos = 0) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type find(CharT ch, size_type pos = 0) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find(ch, pos);
    }

    constexpr size_type find(const_pointer s, size_type pos, size_type count) const {
        return find(basic_cstring_view(s, count), pos);
    }

    constexpr size_type find(const_pointer s, size_type pos = 0) const {
        return find(basic_cstring_view(s), pos);
    }

    constexpr size_type rfind(basic_cstring_view v, size_type pos = npos) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).rfind(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type rfind(CharT ch, size_type pos = npos) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).rfind(ch, pos);
    }

    constexpr size_type rfind(const_pointer s, size_type pos, size_type count) const {
        return rfind(basic_cstring_view(s, count), pos);
    }

    constexpr size_type rfind(const_pointer s, size_type pos = npos) const {
        return rfind(basic_cstring_view(s), pos);
    }

    constexpr size_type find_first_of(basic_cstring_view v, size_type pos = 0) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_first_of(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type find_first_of(CharT ch, size_type pos = 0) const noexcept {
        return find(ch, pos);
    }

    constexpr size_type find_first_of(const_pointer s, size_type pos, size_type count) const {
        return find_first_of(basic_cstring_view(s, count), pos);
    }

    constexpr size_type find_first_of(const_pointer s, size_type pos = 0) const {
        return find_first_of(basic_cstring_view(s), pos);
    }

    constexpr size_type find_last_of(basic_cstring_view v, size_type pos = npos) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_last_of(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type find_last_of(CharT ch, size_type pos = npos) const noexcept {
        return rfind(ch, pos);
    }

    constexpr size_type find_last_of(const_pointer s, size_type pos, size_type count) const {
        return find_last_of(basic_cstring_view(s, count), pos);
    }

    constexpr size_type find_last_of(const_pointer s, size_type pos = npos) const {
        return find_last_of(basic_cstring_view(s), pos);
    }

    constexpr size_type find_first_not_of(basic_cstring_view v, size_type pos = 0) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_first_not_of(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_first_not_of(ch, pos);
    }

    constexpr size_type find_first_not_of(const_pointer s, size_type pos, size_type count) const {
        return find_first_not_of(basic_cstring_view(s, count), pos);
    }

    constexpr size_type find_first_not_of(const_pointer s, size_type pos = 0) const {
        return find_first_not_of(basic_cstring_view(s), pos);
    }

    constexpr size_type find_last_not_of(basic_cstring_view v, size_type pos = npos) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_last_not_of(
            std::basic_string_view<CharT, Traits>(v.data_, v.size_), pos);
    }

    constexpr size_type find_last_not_of(CharT ch, size_type pos = npos) const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_).find_last_not_of(ch, pos);
    }

    constexpr size_type find_last_not_of(const_pointer s, size_type pos, size_type count) const {
        return find_last_not_of(basic_cstring_view(s, count), pos);
    }

    constexpr size_type find_last_not_of(const_pointer s, size_type pos = npos) const {
        return find_last_not_of(basic_cstring_view(s), pos);
    }

    // Conversion to string_view
    constexpr operator std::basic_string_view<CharT, Traits>() const noexcept {
        return std::basic_string_view<CharT, Traits>(data_, size_);
    }
};

// Comparison operators
template<class CharT, class Traits>
constexpr bool operator==(basic_cstring_view<CharT, Traits> lhs,
                         basic_cstring_view<CharT, Traits> rhs) noexcept {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template<class CharT, class Traits>
constexpr auto operator<=>(basic_cstring_view<CharT, Traits> lhs,
                           basic_cstring_view<CharT, Traits> rhs) noexcept {
    int cmp = lhs.compare(rhs);
    return cmp <=> 0;
}

// Stream output
template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              basic_cstring_view<CharT, Traits> v) {
    return os << std::basic_string_view<CharT, Traits>(v.data(), v.size());
}

// Type aliases
using cstring_view = basic_cstring_view<char>;
using wcstring_view = basic_cstring_view<wchar_t>;
using u8cstring_view = basic_cstring_view<char8_t>;
using u16cstring_view = basic_cstring_view<char16_t>;
using u32cstring_view = basic_cstring_view<char32_t>;

// Deduction guides
template<class CharT>
basic_cstring_view(const CharT*) -> basic_cstring_view<CharT>;

template<class CharT>
basic_cstring_view(const CharT*, std::size_t) -> basic_cstring_view<CharT>;

// Literal operators
namespace literals {
namespace cstring_view_literals {

constexpr cstring_view operator""_csv(const char* str, std::size_t len) noexcept {
    return cstring_view(str, len);
}

constexpr wcstring_view operator""_csv(const wchar_t* str, std::size_t len) noexcept {
    return wcstring_view(str, len);
}

constexpr u8cstring_view operator""_csv(const char8_t* str, std::size_t len) noexcept {
    return u8cstring_view(str, len);
}

constexpr u16cstring_view operator""_csv(const char16_t* str, std::size_t len) noexcept {
    return u16cstring_view(str, len);
}

constexpr u32cstring_view operator""_csv(const char32_t* str, std::size_t len) noexcept {
    return u32cstring_view(str, len);
}

} // namespace cstring_view_literals
} // namespace literals

#endif /* CSTRING_VIEW_A4453D98_D8EF_423F_A7A4_5F105FE4D972 */
