#pragma once
#include <cstddef>
#include <utility>
namespace museum::brown {
template <class T> class UniquePtr {
    T *pointer_ = nullptr;

  public:
    UniquePtr() = default;
    explicit UniquePtr(T *p) : pointer_(p) {}
    UniquePtr(const UniquePtr &) = delete;
    UniquePtr &operator=(const UniquePtr &) = delete;
    UniquePtr(UniquePtr &&other) noexcept : pointer_(other.Release()) {}
    UniquePtr &operator=(UniquePtr &&other) noexcept {
        if (this != &other)
            Reset(other.Release());
        return *this;
    }
    UniquePtr &operator=(std::nullptr_t) noexcept {
        Reset();
        return *this;
    }
    ~UniquePtr() { delete pointer_; }
    T &operator*() const { return *pointer_; }
    T *operator->() const { return pointer_; }
    T *Get() const { return pointer_; }
    T *Release() noexcept {
        T *p = pointer_;
        pointer_ = nullptr;
        return p;
    }
    void Reset(T *p = nullptr) noexcept {
        if (p != pointer_) {
            T *old = pointer_;
            pointer_ = p;
            delete old;
        }
    }
    void Swap(UniquePtr &other) noexcept { std::swap(pointer_, other.pointer_); }
};
} // namespace museum::brown
