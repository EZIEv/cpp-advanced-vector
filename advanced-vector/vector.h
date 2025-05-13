#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory& other) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_), capacity_(other.capacity_)
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(const RawMemory& other) = delete;

    RawMemory& operator=(RawMemory&& other) noexcept 
    {
        if (this == &other) {
            return *this;
        }
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
        capacity_ = other.capacity_;
        other.capacity_ = 0;
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) 
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other)
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    Vector& operator=(const Vector& other)
    {
        if (this == &other) {
            return *this;
        }

        if (other.size_ > data_.Capacity()) {
            Vector<T> tmp(other);
            Swap(tmp);
        } else {
            size_t min_size = std::min(size_, other.size_);
            std::copy_n(other.data_.GetAddress(), min_size, data_.GetAddress());
            std::uninitialized_copy_n(other.data_.GetAddress() + min_size, other.size_ - min_size, data_.GetAddress() + min_size);
            if (other.size_ < size_) {
                std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
            }
            size_ = other.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), new_data.Capacity());
    }

    void Swap(Vector& other) noexcept
    {
        if (this == &other) {
            return;
        }
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size)
    {
        if (size_ == new_size) {
            return;
        } else if (size_ < new_size) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack()
    {
        std::destroy_at<T>(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *(Emplace(data_.GetAddress() + size_, std::forward<Args>(args)...));
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t pos_idx = static_cast<size_t>(pos - begin());
        if (size_ == data_.Capacity()) {
            const size_t new_capacity = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);
            
            new (new_data.GetAddress() + pos_idx) T(std::forward<Args>(args)...);
            Reallocate(new_data, pos_idx);
        } else {
            if (pos_idx == size_) {
                new (data_.GetAddress() + pos_idx) T(std::forward<Args>(args)...);
            } else {
                T temp_value(std::forward<Args>(args)...);
                new (data_.GetAddress() + size_) T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + pos_idx, end() - 1, end());
                data_[pos_idx] = std::move(temp_value);
            }
        }
        ++size_;
        return data_.GetAddress() + pos_idx;
    }

    iterator Erase(const_iterator pos) {
        size_t pos_idx = static_cast<size_t>(pos - begin());
        std::destroy_at<T>(data_.GetAddress() + pos_idx);
        std::move(begin() + pos_idx + 1, end(), begin() + pos_idx);
        --size_;
        return data_.GetAddress() + pos_idx;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void Reallocate(RawMemory<T>& new_data) {
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } catch (...) {
            std::destroy_n(new_data.GetAddress() + size_, 1);
            throw;
        }

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), new_data.Capacity());
    }

    void Reallocate(RawMemory<T>& new_data, size_t pos_idx) {
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), pos_idx, new_data.GetAddress());    
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), pos_idx, new_data.GetAddress());
            }
        } catch (...) {
            std::destroy_n(new_data.GetAddress() + pos_idx, 1);
            throw;
        }

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress() + pos_idx, size_ - pos_idx, new_data.GetAddress() + pos_idx + 1);
            } else {
                std::uninitialized_copy_n(data_.GetAddress() + pos_idx, size_ - pos_idx, new_data.GetAddress() + pos_idx + 1);
            }
        } catch (...) {
            std::destroy_n(new_data.GetAddress(), pos_idx + 1);
            throw;
        }

        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), new_data.Capacity());
    }
};