#ifndef ARRAY_MAP_F11AA28E_0319_442A_98FE_6D19716622FE
#define ARRAY_MAP_F11AA28E_0319_442A_98FE_6D19716622FE

#pragma once

#include <vector>
#include <optional>
#include <stdexcept>
#include <concepts>
#include <iterator>
#include <algorithm>
#include <utility>

template<std::integral Key, typename T>
class array_map {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    
private:
    struct slot {
        std::optional<T> value;
        bool occupied = false;
    };
    
    std::vector<slot> data_;
    size_type size_ = 0;
    Key min_key_ = 0;
    Key max_key_ = 0;
    bool empty_map_ = true;
    
    // Convert key to index
    size_type key_to_index(Key k) const {
        if (empty_map_) return 0;
        return static_cast<size_type>(k - min_key_);
    }
    
    // Ensure capacity for a given key
    void ensure_capacity(Key k) {
        if (empty_map_) {
            min_key_ = k;
            max_key_ = k;
            data_.resize(1);
            empty_map_ = false;
        } else {
            Key new_min = std::min(min_key_, k);
            Key new_max = std::max(max_key_, k);
            
            if (new_min != min_key_ || new_max != max_key_) {
                size_type new_size = static_cast<size_type>(new_max - new_min) + 1;
                std::vector<slot> new_data(new_size);
                
                // Copy existing data to new positions
                for (Key i = min_key_; i <= max_key_; ++i) {
                    size_type old_idx = key_to_index(i);
                    if (data_[old_idx].occupied) {
                        size_type new_idx = static_cast<size_type>(i - new_min);
                        new_data[new_idx] = std::move(data_[old_idx]);
                    }
                }
                
                data_ = std::move(new_data);
                min_key_ = new_min;
                max_key_ = new_max;
            }
        }
    }
    
public:
    // Iterator implementation
    class iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = array_map::value_type;
        using difference_type = array_map::difference_type;
        using pointer = value_type*;
        using reference = value_type&;
        
    private:
        array_map* map_;
        Key current_key_;
        mutable std::optional<value_type> cached_value_;
        
        void update_cache() const {
            if (map_ && current_key_ >= map_->min_key_ && current_key_ <= map_->max_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.emplace(current_key_, map_->data_[idx].value.value());
                }
            }
        }
        
        void find_next_occupied() {
            if (!map_ || map_->empty_map_) return;
            
            while (current_key_ <= map_->max_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.reset();
                    return;
                }
                ++current_key_;
            }
            // Reached end
            current_key_ = map_->max_key_ + 1;
            cached_value_.reset();
        }
        
        void find_prev_occupied() {
            if (!map_ || map_->empty_map_) return;
            
            while (current_key_ >= map_->min_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.reset();
                    return;
                }
                --current_key_;
            }
        }
        
    public:
        iterator(array_map* map, Key key, bool find_occupied = false) 
            : map_(map), current_key_(key) {
            if (find_occupied) {
                find_next_occupied();
            }
        }
        
        reference operator*() const {
            update_cache();
            return *cached_value_;
        }
        
        pointer operator->() const {
            update_cache();
            return &(*cached_value_);
        }
        
        iterator& operator++() {
            ++current_key_;
            find_next_occupied();
            return *this;
        }
        
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        iterator& operator--() {
            --current_key_;
            find_prev_occupied();
            return *this;
        }
        
        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }
        
        bool operator==(const iterator& other) const {
            return map_ == other.map_ && current_key_ == other.current_key_;
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
        
        friend class array_map;
    };
    
    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = array_map::value_type;
        using difference_type = array_map::difference_type;
        using pointer = const value_type*;
        using reference = const value_type&;
        
    private:
        const array_map* map_;
        Key current_key_;
        mutable std::optional<value_type> cached_value_;
        
        void update_cache() const {
            if (map_ && current_key_ >= map_->min_key_ && current_key_ <= map_->max_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.emplace(current_key_, map_->data_[idx].value.value());
                }
            }
        }
        
        void find_next_occupied() {
            if (!map_ || map_->empty_map_) return;
            
            while (current_key_ <= map_->max_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.reset();
                    return;
                }
                ++current_key_;
            }
            current_key_ = map_->max_key_ + 1;
            cached_value_.reset();
        }
        
        void find_prev_occupied() {
            if (!map_ || map_->empty_map_) return;
            
            while (current_key_ >= map_->min_key_) {
                size_type idx = map_->key_to_index(current_key_);
                if (map_->data_[idx].occupied) {
                    cached_value_.reset();
                    return;
                }
                --current_key_;
            }
        }
        
    public:
        const_iterator(const array_map* map, Key key, bool find_occupied = false) 
            : map_(map), current_key_(key) {
            if (find_occupied) {
                find_next_occupied();
            }
        }
        
        const_iterator(const iterator& it) 
            : map_(it.map_), current_key_(it.current_key_) {}
        
        reference operator*() const {
            update_cache();
            return *cached_value_;
        }
        
        pointer operator->() const {
            update_cache();
            return &(*cached_value_);
        }
        
        const_iterator& operator++() {
            ++current_key_;
            find_next_occupied();
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        const_iterator& operator--() {
            --current_key_;
            find_prev_occupied();
            return *this;
        }
        
        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            return map_ == other.map_ && current_key_ == other.current_key_;
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };
    
    // Constructors
    array_map() = default;
    
    array_map(std::initializer_list<value_type> init) {
        for (const auto& [key, value] : init) {
            insert({key, value});
        }
    }
    
    // Capacity
    bool empty() const noexcept {
        return size_ == 0;
    }
    
    size_type size() const noexcept {
        return size_;
    }
    
    size_type max_size() const noexcept {
        return data_.max_size();
    }
    
    // Element access
    T& at(const Key& key) {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            throw std::out_of_range("array_map::at");
        }
        
        size_type idx = key_to_index(key);
        if (!data_[idx].occupied) {
            throw std::out_of_range("array_map::at");
        }
        
        return data_[idx].value.value();
    }
    
    const T& at(const Key& key) const {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            throw std::out_of_range("array_map::at");
        }
        
        size_type idx = key_to_index(key);
        if (!data_[idx].occupied) {
            throw std::out_of_range("array_map::at");
        }
        
        return data_[idx].value.value();
    }
    
    T& operator[](const Key& key) {
        ensure_capacity(key);
        size_type idx = key_to_index(key);
        
        if (!data_[idx].occupied) {
            data_[idx].value.emplace();
            data_[idx].occupied = true;
            ++size_;
        }
        
        return data_[idx].value.value();
    }
    
    // Iterators
    iterator begin() {
        if (empty_map_) {
            return end();
        }
        return iterator(this, min_key_, true);
    }
    
    const_iterator begin() const {
        if (empty_map_) {
            return end();
        }
        return const_iterator(this, min_key_, true);
    }
    
    const_iterator cbegin() const {
        return begin();
    }
    
    iterator end() {
        if (empty_map_) {
            return iterator(this, 0);
        }
        return iterator(this, max_key_ + 1);
    }
    
    const_iterator end() const {
        if (empty_map_) {
            return const_iterator(this, 0);
        }
        return const_iterator(this, max_key_ + 1);
    }
    
    const_iterator cend() const {
        return end();
    }
    
    // Modifiers
    void clear() noexcept {
        data_.clear();
        size_ = 0;
        empty_map_ = true;
        min_key_ = 0;
        max_key_ = 0;
    }
    
    std::pair<iterator, bool> insert(const value_type& value) {
        const auto& [key, val] = value;
        ensure_capacity(key);
        size_type idx = key_to_index(key);
        
        if (data_[idx].occupied) {
            return {iterator(this, key), false};
        }
        
        data_[idx].value = val;
        data_[idx].occupied = true;
        ++size_;
        
        return {iterator(this, key), true};
    }
    
    std::pair<iterator, bool> insert(value_type&& value) {
        auto [key, val] = std::move(value);
        ensure_capacity(key);
        size_type idx = key_to_index(key);
        
        if (data_[idx].occupied) {
            return {iterator(this, key), false};
        }
        
        data_[idx].value = std::move(val);
        data_[idx].occupied = true;
        ++size_;
        
        return {iterator(this, key), true};
    }
    
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        value_type temp(std::forward<Args>(args)...);
        return insert(std::move(temp));
    }
    
    iterator erase(iterator pos) {
        Key key = pos.current_key_;
        if (empty_map_ || key < min_key_ || key > max_key_) {
            return end();
        }
        
        size_type idx = key_to_index(key);
        if (data_[idx].occupied) {
            data_[idx].occupied = false;
            data_[idx].value.reset();
            --size_;
        }
        
        iterator next = pos;
        ++next;
        return next;
    }
    
    size_type erase(const Key& key) {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            return 0;
        }
        
        size_type idx = key_to_index(key);
        if (!data_[idx].occupied) {
            return 0;
        }
        
        data_[idx].occupied = false;
        data_[idx].value.reset();
        --size_;
        return 1;
    }
    
    void swap(array_map& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(min_key_, other.min_key_);
        std::swap(max_key_, other.max_key_);
        std::swap(empty_map_, other.empty_map_);
    }
    
    // Lookup
    size_type count(const Key& key) const {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            return 0;
        }
        
        size_type idx = key_to_index(key);
        return data_[idx].occupied ? 1 : 0;
    }
    
    iterator find(const Key& key) {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            return end();
        }
        
        size_type idx = key_to_index(key);
        if (!data_[idx].occupied) {
            return end();
        }
        
        return iterator(this, key);
    }
    
    const_iterator find(const Key& key) const {
        if (empty_map_ || key < min_key_ || key > max_key_) {
            return end();
        }
        
        size_type idx = key_to_index(key);
        if (!data_[idx].occupied) {
            return end();
        }
        
        return const_iterator(this, key);
    }
    
    bool contains(const Key& key) const {
        return find(key) != end();
    }
    
    // Custom functionality
    void shrink_to_fit() {
        if (empty()) {
            data_.clear();
            data_.shrink_to_fit();
            empty_map_ = true;
            return;
        }
        
        // Find actual min and max occupied keys
        Key new_min = max_key_;
        Key new_max = min_key_;
        
        for (Key k = min_key_; k <= max_key_; ++k) {
            size_type idx = key_to_index(k);
            if (data_[idx].occupied) {
                new_min = std::min(new_min, k);
                new_max = std::max(new_max, k);
            }
        }
        
        if (new_min > new_max) {
            // No occupied slots
            clear();
            return;
        }
        
        if (new_min != min_key_ || new_max != max_key_) {
            size_type new_size = static_cast<size_type>(new_max - new_min) + 1;
            std::vector<slot> new_data(new_size);
            
            for (Key k = new_min; k <= new_max; ++k) {
                size_type old_idx = key_to_index(k);
                if (data_[old_idx].occupied) {
                    size_type new_idx = static_cast<size_type>(k - new_min);
                    new_data[new_idx] = std::move(data_[old_idx]);
                }
            }
            
            data_ = std::move(new_data);
            min_key_ = new_min;
            max_key_ = new_max;
        }
        
        data_.shrink_to_fit();
    }
    
    // Get the underlying capacity
    size_type capacity() const {
        return data_.capacity();
    }
    
    // Get range of keys
    std::pair<Key, Key> key_range() const {
        if (empty_map_) {
            return {0, 0};
        }
        return {min_key_, max_key_};
    }
};

// Non-member functions
template<std::integral Key, typename T>
void swap(array_map<Key, T>& lhs, array_map<Key, T>& rhs) noexcept {
    lhs.swap(rhs);
}

template<std::integral Key, typename T>
bool operator==(const array_map<Key, T>& lhs, const array_map<Key, T>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    
    for (const auto& [key, value] : lhs) {
        auto it = rhs.find(key);
        if (it == rhs.end() || it->second != value) {
            return false;
        }
    }
    
    return true;
}

template<std::integral Key, typename T>
bool operator!=(const array_map<Key, T>& lhs, const array_map<Key, T>& rhs) {
    return !(lhs == rhs);
}

#endif /* ARRAY_MAP_F11AA28E_0319_442A_98FE_6D19716622FE */
