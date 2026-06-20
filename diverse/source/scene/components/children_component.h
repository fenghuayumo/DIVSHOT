#pragma once

#include <entt/entity/fwd.hpp>
#include <cstdint>
#include <cstring>

namespace diverse
{

    // Small vector optimization: inline storage for up to 4 children
    // This reduces heap allocations for entities with few children
    class Children
    {
    public:
        static constexpr size_t INLINE_CAPACITY = 4;

        Children()
            : data_(inline_data_)
            , size_(0)
            , capacity_(INLINE_CAPACITY)
        {
            std::memset(inline_data_, 0, sizeof(inline_data_));
        }

        ~Children()
        {
            if (data_ != inline_data_)
            {
                delete[] data_;
            }
        }

        // Disable copy, enable move
        Children(const Children&) = delete;
        Children& operator=(const Children&) = delete;

        Children(Children&& other) noexcept
            : size_(other.size_)
            , capacity_(other.capacity_)
        {
            if (other.data_ == other.inline_data_)
            {
                // Other is using inline storage
                std::memcpy(inline_data_, other.inline_data_, sizeof(inline_data_));
                data_ = inline_data_;
            }
            else
            {
                // Other is using heap storage
                data_ = other.data_;
                other.data_ = other.inline_data_;
                other.capacity_ = INLINE_CAPACITY;
            }
            other.size_ = 0;
        }

        Children& operator=(Children&& other) noexcept
        {
            if (this != &other)
            {
                // Clean up current heap storage if any
                if (data_ != inline_data_)
                {
                    delete[] data_;
                }

                size_ = other.size_;
                capacity_ = other.capacity_;

                if (other.data_ == other.inline_data_)
                {
                    std::memcpy(inline_data_, other.inline_data_, sizeof(inline_data_));
                    data_ = inline_data_;
                }
                else
                {
                    data_ = other.data_;
                    other.data_ = other.inline_data_;
                    other.capacity_ = INLINE_CAPACITY;
                }
                other.size_ = 0;
            }
            return *this;
        }

        size_t size() const { return size_; }
        bool is_empty() const { return size_ == 0; }

        void push(entt::entity child)
        {
            if (size_ >= capacity_)
            {
                grow();
            }
            data_[size_++] = child;
        }

        void remove(entt::entity child)
        {
            for (size_t i = 0; i < size_; ++i)
            {
                if (data_[i] == child)
                {
                    // Shift remaining elements
                    for (size_t j = i; j < size_ - 1; ++j)
                    {
                        data_[j] = data_[j + 1];
                    }
                    --size_;
                    return;
                }
            }
        }

        void clear()
        {
            size_ = 0;
        }

        entt::entity operator[](size_t index) const
        {
            return data_[index];
        }

        entt::entity* begin() { return data_; }
        entt::entity* end() { return data_ + size_; }
        const entt::entity* begin() const { return data_; }
        const entt::entity* end() const { return data_ + size_; }

    private:
        void grow()
        {
            size_t new_capacity = capacity_ * 2;
            entt::entity* new_data = new entt::entity[new_capacity];

            std::memcpy(new_data, data_, size_ * sizeof(entt::entity));

            if (data_ != inline_data_)
            {
                delete[] data_;
            }

            data_ = new_data;
            capacity_ = static_cast<uint8_t>(new_capacity);
        }

        entt::entity inline_data_[INLINE_CAPACITY];
        entt::entity* data_;
        uint8_t size_;
        uint8_t capacity_;
    };

}
