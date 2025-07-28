#pragma once
#include "core/memory.h"

namespace diverse
{
    template <class T>
    class Vector
    {
    public:
        Vector(Arena* arena = nullptr)
            : m_Arena(arena)
        {
        }

        Vector(const Vector<T>& another_vector);                       // copy constructor
        Vector(size_t size, Arena* arena = nullptr, T initial = T{}); // constructor based on capacity and a default value
        Vector(std::initializer_list<T> values, Arena* arena = nullptr);
        Vector(Vector<T>&& other) noexcept;

        ~Vector()
        {
            if (!m_Arena)
                delete[] m_Data;
        }

        Vector<T>& operator=(const Vector<T>&); // copy assignment

        size_t Size() const { return m_CurrentIndex; }
        size_t Capacity() const { return m_Size; }

        T& Emplace(const T& element); // pass element by constant reference
        T& Emplace(T&& element);

        template <typename... Args>
        T& EmplaceBack(Args&&... args);

        T& EmplaceBack();
        T& Back();
        void Pop();
        void Clear(bool deleteData = false);

        bool Empty() const { return m_CurrentIndex == 0; }

        bool operator==(const Vector<T>& other) const;
        bool operator!=(const Vector<T>& other) const;

        class Iterator
        {
        public:
            Iterator(T* ptr)
                : ptr(ptr)
            {
            }
            Iterator operator++()
            {
                ++ptr;
                return *this;
            }
            bool operator!=(const Iterator& other) const { return ptr != other.ptr; }
            T& operator*() const { return *ptr; }
            Iterator operator+(int offset) const { return Iterator(ptr + offset); }

        private:
            T* ptr;
        };

        Iterator begin() const { return Iterator(m_Data); }
        Iterator end() const { return Iterator(m_Data + m_CurrentIndex); }

        T& operator[](const size_t index);
        const T& operator[](const size_t index) const;
        T* Data() { return m_Data; }

        void Reserve(const size_t size);

    private:
        T* m_Data = nullptr;
        size_t m_Size = 0;
        size_t m_CurrentIndex = 0;
        Arena* m_Arena = nullptr;
    };

    template <class T>
    Vector<T>::Vector(const Vector<T>& other)
    {
        if (!m_Arena)
            delete[] m_Data; // Delete before copying everything from another vector

        // Copy everything from another vector
        m_CurrentIndex = other.Size();
        m_Size = other.Capacity();
        if (m_Arena)
            m_Data = PushArrayNoZero(m_Arena, T, m_Size);
        else
            m_Data = new T[m_Size];

        for (size_t i = 0; i < m_CurrentIndex; ++i)
            m_Data[i] = other[i];
    }

    template <class T>
    Vector<T>::Vector(Vector<T>&& other) noexcept
        : m_Data(other.m_Data)
        , m_Size(other.m_Size)
        , m_CurrentIndex(other.m_CurrentIndex)
    {
    }

    template <class T>
    Vector<T>::Vector(std::initializer_list<T> values, Arena* arena)
        : m_Arena(arena)
    {
        if (!m_Arena)
            delete[] m_Data; // Delete before copying everything from another vector

        // Copy everything from another vector
        m_CurrentIndex = values.size();
        m_Size = values.size();

        if (arena)
            m_Data = PushArrayNoZero(m_Arena, T, m_Size);
        else
            m_Data = new T[m_Size];

        size_t index = 0;
        for (auto& value : values)
            m_Data[index++] = value;
    }

    template <class T>
    Vector<T>::Vector(size_t size, Arena* arena, T initial)
        : m_Size(size)
        , m_CurrentIndex(size)
        , m_Arena(arena)
    {
        if (m_Arena)
            m_Data = PushArrayNoZero(m_Arena, T, m_Size);
        else
            m_Data = new T[size];

        for (size_t i = 0; i < size; ++i)
            m_Data[i] = initial; // initialize
    }

    // Copy assignment
    template <class T>
    Vector<T>& Vector<T>::operator=(const Vector<T>& other)
    {
        if (!m_Arena)
            delete[] m_Data; // Delete before copying everything from another vector

        // Copy everything from another vector
        m_CurrentIndex = other.Size();
        m_Size = other.Capacity();

        if (m_Arena)
            m_Data = PushArrayNoZero(m_Arena, T, m_Size);
        else
            m_Data = new T[m_Size];

        for (size_t i = 0; i < m_Size; ++i)
            m_Data[i] = other[i];

        return *this;
    }

    template <class T>
    T& Vector<T>::operator[](const size_t index)
    {
        DS_ASSERT(index < m_CurrentIndex, "Index must be less than vector's size");

        return m_Data[index];
    }

    template <class T>
    const T& Vector<T>::operator[](const size_t index) const
    {
        DS_ASSERT(index < m_CurrentIndex, "Index must be less than vector's size");
        return m_Data[index];
    }

    template <class T>
    T& Vector<T>::Emplace(const T& element)
    {
        // If no cacacity, increase capacity
        if (m_CurrentIndex == m_Size)
        {
            if (m_Size == 0) // handing initial when
                Reserve(4);
            else
                Reserve(m_Size * 2);
        }

        // Append an element to the array
        m_Data[m_CurrentIndex] = element;
        m_CurrentIndex++;

        return m_Data[m_CurrentIndex - 1];
    }

    template <class T>
    T& Vector<T>::Emplace(T&& element)
    {
        // If no cacacity, increase capacity
        if (m_CurrentIndex == m_Size)
        {
            if (m_Size == 0) // handing initial when
                Reserve(4);
            else
                Reserve(m_Size * 2);
        }

        // Append an element to the array
        m_Data[m_CurrentIndex] = std::move(element);
        m_CurrentIndex++;

        return m_Data[m_CurrentIndex - 1];
    }

    template <typename T>
    template <typename... Args>
    T& Vector<T>::EmplaceBack(Args&&... args)
    {
        // If no cacacity, increase capacity
        if (m_CurrentIndex == m_Size)
        {
            if (m_Size == 0) // handing initial when
                Reserve(4);
            else
                Reserve(m_Size * 2);
        }

        // Append an element to the array
        m_Data[m_CurrentIndex] = T(std::forward<Args>(args)...);
        m_CurrentIndex++;

        return m_Data[m_CurrentIndex - 1];
    }

    template <class T>
    T& Vector<T>::EmplaceBack()
    {
        // If no cacacity, increase capacity
        if (m_CurrentIndex == m_Size)
        {
            if (m_Size == 0) // handing initial when
                Reserve(4);
            else
                Reserve(m_Size * 2);
        }

        // Append an element to the array
        auto& element = m_Data[m_CurrentIndex];
        m_CurrentIndex++;

        return element;
    }

    template <class T>
    T& Vector<T>::Back()
    {
        return m_Data[m_CurrentIndex - 1];
    }

    template <class T>
    void Vector<T>::Pop()
    {
        if (m_CurrentIndex > 0) // Nothing to pop otherwise
        {
            m_CurrentIndex--;
        }
        else
            DS_ASSERT(false, "Nothing to pop");
    }

    template <class T>
    void Vector<T>::Clear(bool deleteData)
    {
        if (deleteData)
        {
            if (!m_Arena)
                delete[] m_Data;
            m_CurrentIndex = 0;
            m_Size = 0;
        }
        else
        {
            m_CurrentIndex = 0;
        }
    }

    template <class T>
    inline void Vector<T>::Reserve(const size_t capacity)
    {
        // Handle case when given capacity is less than equal to size. (No need to reallocate)
        if (capacity > m_CurrentIndex)
        {
            // Reserves memory of size capacity for the vector_
            T* temp;
            if (m_Arena)
                temp = PushArrayNoZero(m_Arena, T, capacity);
            else
                temp = new T[capacity];

            // Move previous elements to this memory
            for (size_t i = 0; i < m_Size; ++i)
                temp[i] = std::move(m_Data[i]);

            if (!m_Arena)
                delete[] m_Data; // Delete old vector

            m_Size = capacity;
            m_Data = temp; // Copy assignment
        }
    }
}
