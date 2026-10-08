#pragma once
#include <cstddef>
#include <stdexcept>
#include <vector>
namespace museum::red {
template <class T> class Deque {
    std::vector<T> front_, back_;

  public:
    bool Empty() const { return Size() == 0; }
    std::size_t Size() const { return front_.size() + back_.size(); }
    T &operator[](std::size_t i) {
        return i < front_.size() ? front_[front_.size() - 1 - i] : back_[i - front_.size()];
    }
    const T &operator[](std::size_t i) const {
        return i < front_.size() ? front_[front_.size() - 1 - i] : back_[i - front_.size()];
    }
    T &At(std::size_t i) {
        if (i >= Size())
            throw std::out_of_range("deque index");
        return (*this)[i];
    }
    const T &At(std::size_t i) const {
        if (i >= Size())
            throw std::out_of_range("deque index");
        return (*this)[i];
    }
    T &Front() { return At(0); }
    const T &Front() const { return At(0); }
    T &Back() { return At(Size() - 1); }
    const T &Back() const { return At(Size() - 1); }
    void PushFront(const T &value) { front_.push_back(value); }
    void PushBack(const T &value) { back_.push_back(value); }
};
} // namespace museum::red
