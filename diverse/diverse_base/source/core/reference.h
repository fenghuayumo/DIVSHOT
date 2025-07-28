#pragma once
#include "core.h"
#include "reference_counter.h"
#include "core/memory.h"
#include "core/ds_log.h"

namespace diverse
{
    class DS_EXPORT RefCount
    {
    public:
        RefCount();
        ~RefCount();

        inline bool isReferenced() const
        {
            return m_RefcountInit.get() < 1;
        }
        bool initRef();

        // Returns false if refcount is at zero and didn't get increased
        bool reference();
        bool unreference();

        bool weakReference();
        bool weakUnreference();

        int getReferenceCount() const;
        int getWeakReferenceCount() const;

    private:
        ReferenceCounter m_Refcount;
        ReferenceCounter m_RefcountInit;
        ReferenceCounter m_WeakRefcount;
    };

    template <class T>
    class Reference
    {
    public:
        Reference() noexcept
            : m_Ptr(nullptr)
        {
        }

        Reference(std::nullptr_t) noexcept
            : m_Ptr(nullptr)
            , m_Counter(nullptr)
        {
        }

        explicit Reference(T* ptr) noexcept
            : m_Ptr(nullptr)
            , m_Counter(nullptr)
        {
            if(ptr)
                refPointer(ptr);
        }

        Reference(const Reference<T>& other) noexcept
        {
            m_Ptr     = nullptr;
            m_Counter = nullptr;

            ref(other);
        }

        Reference(Reference<T>&& rhs) noexcept
        {
            m_Ptr     = nullptr;
            m_Counter = nullptr;

            ref(rhs);
        }

        template <typename U>
        inline Reference(const Reference<U>& moving) noexcept
        {
            U* movingPtr = moving.get();

            T* castPointer = static_cast<T*>(movingPtr);

            unref();

            if(castPointer != nullptr)
            {
                if(moving.get() == m_Ptr)
                    return;

                if(moving.getCounter() && moving.get())
                {
                    m_Ptr     = castPointer;
                    m_Counter = moving.getCounter();
                    m_Counter->reference();
                }
            }
            else
            {
                DS_LOG_ERROR("Failed to cast Reference");
            }
        }

        ~Reference() noexcept
        {
            unref();
        }

        // Access to smart pointer state
        inline T* get() const
        {
            return m_Ptr;
        }
        inline RefCount* getCounter() const
        {
            return m_Counter;
        }

        inline int use_count() const
		{
			return m_Counter ? m_Counter->getReferenceCount() : 0;
		}

        inline T* release() noexcept
        {
            T* tmp = nullptr;

            if(m_Counter->unreference())
            {
                delete m_Counter;
                m_Counter = nullptr;
            }

            std::swap(tmp, m_Ptr);
            m_Ptr = nullptr;

            return tmp;
        }

        inline void reset(T* p_ptr = nullptr)
        {
            unref();

            m_Ptr     = p_ptr;
            m_Counter = nullptr;

            if(m_Ptr != nullptr)
            {
                m_Counter = new RefCount();
                m_Counter->initRef();
            }
        }

        inline void operator=(Reference const& rhs)
        {
            ref(rhs);
        }

        inline Reference& operator=(Reference&& rhs) noexcept
        {
            ref(rhs);
            return *this;
        }

        inline Reference& operator=(T* newData)
        {
            reset(newData);
            return *this;
        }

        template <typename U>
        inline Reference& operator=(const Reference<U>& moving)
        {
            U* movingPtr = moving.get();

            T* castPointer = dynamic_cast<T*>(movingPtr);

            unref();

            if(castPointer != nullptr)
            {
                if(moving.getCounter() && moving.get())
                {
                    m_Ptr     = moving.get();
                    m_Counter = moving.getCounter();
                    m_Counter->reference();
                }
            }
            else
            {
                DS_LOG_ERROR("Failed to cast Reference");
            }

            return *this;
        }

        inline operator T*() const
        {
            return m_Ptr;
        }

        inline Reference& operator=(std::nullptr_t)
        {
            reset();
            return *this;
        }

        // Const correct access owned object
        inline T* operator->() const
        {
            return &*m_Ptr;
        }
        inline T& operator*() const
        {
            return *m_Ptr;
        }

        inline T& operator[](int index)
        {
            assert(m_Ptr);
            return m_Ptr[index];
        }

        inline explicit constexpr operator bool() const
        {
            return m_Ptr != nullptr;
        }

        inline constexpr bool operator==(const T* p_ptr) const
        {
            return m_Ptr == p_ptr;
        }
        inline constexpr bool operator!=(const T* p_ptr) const
        {
            return m_Ptr != p_ptr;
        }
        inline constexpr bool operator<(const Reference<T>& p_r) const
        {
            return m_Ptr < p_r.m_Ptr;
        }
        inline constexpr bool operator==(const Reference<T>& p_r) const
        {
            return m_Ptr == p_r.m_Ptr;
        }
        inline constexpr bool operator!=(const Reference<T>& p_r) const
        {
            return m_Ptr != p_r.m_Ptr;
        }

        inline void swap(Reference& other) noexcept
        {
            std::swap(m_Ptr, other.m_Ptr);
            std::swap(m_Counter, other.m_Counter);
        }

        template <typename U>
        inline Reference<U> As() const
        {
            return Reference<U>(*this);
        }

    private:
        inline void ref(const Reference& p_from)
        {
            if(p_from.m_Ptr == m_Ptr)
                return;

            unref();

            m_Counter = nullptr;
            m_Ptr     = nullptr;

            if(p_from.getCounter() && p_from.get())
            {
                m_Ptr     = p_from.get();
                m_Counter = p_from.getCounter();
                m_Counter->reference();
            }
        }

        inline void refPointer(T* ptr)
        {
            DS_ASSERT(ptr, "Creating shared ptr with nullptr");

            m_Ptr     = ptr;
            m_Counter = new RefCount();
            m_Counter->initRef();
        }

        inline void unref()
        {
            if(m_Counter != nullptr)
            {
                if(m_Counter->unreference())
                {
                    delete m_Ptr;

                    if(m_Counter->getWeakReferenceCount() == 0)
                    {
                        delete m_Counter;
                    }

                    m_Ptr     = nullptr;
                    m_Counter = nullptr;
                }
            }
        }

        RefCount* m_Counter = nullptr;
        T* m_Ptr            = nullptr;
    };

    template <class T>
    class DS_EXPORT WeakReference
    {
    public:
        WeakReference() noexcept
            : m_Ptr(nullptr)
            , m_Counter(nullptr)
        {
        }

        WeakReference(std::nullptr_t) noexcept
            : m_Ptr(nullptr)
            , m_Counter(nullptr)
        {
        }

        WeakReference(const WeakReference<T>& rhs) noexcept
            : m_Ptr(rhs.m_Ptr)
            , m_Counter(rhs.m_Counter)
        {
            addRef();
        }

        explicit WeakReference(T* ptr) noexcept
            : m_Ptr(ptr)
        {
            DS_ASSERT(ptr, "Creating weak ptr with nullptr");

            m_Counter = new RefCount();
            m_Counter->weakReference();
        }

        template <class U>
        WeakReference(const WeakReference<U>& rhs) noexcept
            : m_Ptr(rhs.m_Ptr)
            , m_Counter(rhs.m_Counter)
        {
            addRef();
        }

        WeakReference(const Reference<T>& rhs) noexcept
            : m_Ptr(rhs.get())
            , m_Counter(rhs.m_Counter)
        {
            addRef();
        }

        ~WeakReference() noexcept
        {
            if(m_Counter->weakUnreference())
            {
                delete m_Ptr;
            }
        }

        void addRef()
        {
            m_Counter->weakReference();
        }

        bool expired() const
        {
            return m_Counter ? m_Counter->getReferenceCount() <= 0 : true;
        }

        Reference<T> Lock() const
        {
            if(expired())
                return Reference<T>();
            else
                return Reference<T>(m_Ptr);
        }

        inline T* operator->() const
        {
            return &*m_Ptr;
        }
        inline T& operator*() const
        {
            return *m_Ptr;
        }

        inline T& operator[](int index)
        {
            assert(m_Ptr);
            return m_Ptr[index];
        }

        inline explicit operator bool() const
        {
            return m_Ptr != nullptr;
        }
        inline bool operator==(const T* p_ptr) const
        {
            return m_Ptr == p_ptr;
        }
        inline bool operator!=(const T* p_ptr) const
        {
            return m_Ptr != p_ptr;
        }
        inline bool operator<(const WeakReference<T>& p_r) const
        {
            return m_Ptr < p_r.m_Ptr;
        }
        inline bool operator==(const WeakReference<T>& p_r) const
        {
            return m_Ptr == p_r.m_Ptr;
        }
        inline bool operator!=(const WeakReference<T>& p_r) const
        {
            return m_Ptr != p_r.m_Ptr;
        }

    private:
        T* m_Ptr;
        RefCount* m_Counter = nullptr;
    };

    template <class T>
    class DS_EXPORT Owned
    {
    public:
        explicit Owned(std::nullptr_t)
        {
            m_Ptr = nullptr;
        }

        Owned(T* ptr = nullptr)
        {
            m_Ptr = ptr;
        }

        template <typename U>
        explicit Owned(U* ptr)
        {
            m_Ptr = dynamic_cast<T*>(ptr);
        }

        ~Owned()
        {
            delete m_Ptr;
        }

        Owned(Owned const&)            = delete;
        Owned& operator=(Owned const&) = delete;

        inline Owned(Owned&& moving) noexcept
        {
            moving.swap(*this);
        }

        inline Owned& operator=(Owned&& moving) noexcept
        {
            moving.swap(*this);
            return *this;
        }

        template <typename U>
        inline Owned(Owned<U>&& moving)
        {
            Owned<T> tmp(moving.release());
            tmp.swap(*this);
        }
        template <typename U>
        inline Owned& operator=(Owned<U>&& moving)
        {
            Owned<T> tmp(moving.release());
            tmp.swap(*this);
            return *this;
        }

        inline Owned& operator=(std::nullptr_t)
        {
            reset();
            return *this;
        }

        // Const correct access owned object
        T* operator->() const
        {
            return m_Ptr;
        }
        T& operator*() const
        {
            return *m_Ptr;
        }

        // Access to smart pointer state
        T* get() const
        {
            return m_Ptr;
        }
        explicit operator bool() const
        {
            return m_Ptr;
        }

        // Modify object state
        inline T* release()
        {
            T* result = nullptr;
            std::swap(result, m_Ptr);
            return result;
        }

        inline void reset()
        {
            T* tmp = release();
            delete tmp;
        }

        inline void swap(Owned& src) noexcept
        {
            std::swap(m_Ptr, src.m_Ptr);
        }

    private:
        T* m_Ptr = nullptr;
    };

    template <typename T>
    void swap(Owned<T>& lhs, Owned<T>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

#define CUSTOM_SMART_PTR
#ifdef CUSTOM_SMART_PTR

    template <class T>
    using SharedPtr = Reference<T>;

    template <typename T, typename... Args>
    SharedPtr<T> createSharedPtr(Args&&... args)
    {
        auto ptr = new T(std::forward<Args>(args)...);

        return Reference<T>(ptr);
    }

    template <class T>
    using UniquePtr = Owned<T>;

    template <typename T, typename... Args>
    UniquePtr<T> createUniquePtr(Args&&... args)
    {
        auto ptr = new T(std::forward<Args>(args)...);
        return Owned<T>(ptr);
    }

    template <class T>
    using WeakPtr = WeakReference<T>;
#else
    template <class T>
    using SharedPtr = std::shared_ptr<T>;

    template <typename T, typename... Args>
    SharedPtr<T> createSharedPtr(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <class T>
    using WeakPtr = std::weak_ptr<T>;

    template <class T>
    using UniquePtr = std::unique_ptr<T>;

    template <typename T, typename... Args>
    UniquePtr<T> createUniquePtr(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }
#endif
}
#ifdef CUSTOM_SMART_PTR
namespace std
{
    template <typename T>
    struct hash<diverse::Reference<T>>
    {
        size_t operator()(const diverse::Reference<T>& x) const
        {
            return hash<T*>()(x.get());
        }
    };
}
#endif
