#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace museum::black {
inline std::int64_t CheckedSum(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
        throw std::overflow_error("Overflow!");
    return a + b;
}
template <class T, class... U> bool EqualsToOneOf(const T &x, const U &...ys) {
    return ((x == ys) || ...);
}
template <class F, class... T> void ApplyToMany(F &&f, T &&...values) {
    (static_cast<void>(f(std::forward<T>(values))), ...);
}

class BadOptionalAccess : public std::logic_error {
  public:
    BadOptionalAccess() : std::logic_error("empty Optional") {}
};
template <class T> class Optional {
    alignas(T) std::byte bytes_[sizeof(T)];
    bool present_ = false;
    T *Ptr() { return std::launder(reinterpret_cast<T *>(bytes_)); }
    const T *Ptr() const { return std::launder(reinterpret_cast<const T *>(bytes_)); }
    template <class U> void Construct(U &&value) {
        ::new (static_cast<void *>(bytes_)) T(std::forward<U>(value));
        present_ = true;
    }
    template <class U> void Assign(U &&value) {
        if (present_)
            *Ptr() = std::forward<U>(value);
        else
            Construct(std::forward<U>(value));
    }

  public:
    Optional() = default;
    Optional(const T &x) { Construct(x); }
    Optional(T &&x) { Construct(std::move(x)); }
    Optional(const Optional &x) {
        if (x.present_)
            Construct(*x);
    }
    Optional(Optional &&x) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (x.present_)
            Construct(std::move(*x));
    }
    ~Optional() { Reset(); }
    Optional &operator=(const Optional &x) {
        if (this != &x) {
            if (x)
                Assign(*x);
            else
                Reset();
        }
        return *this;
    }
    Optional &operator=(Optional &&x) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                               std::is_nothrow_move_assignable_v<T>) {
        if (this != &x) {
            if (x)
                Assign(std::move(*x));
            else
                Reset();
        }
        return *this;
    }
    Optional &operator=(const T &x) {
        Assign(x);
        return *this;
    }
    Optional &operator=(T &&x) {
        Assign(std::move(x));
        return *this;
    }
    bool HasValue() const noexcept { return present_; }
    explicit operator bool() const noexcept { return present_; }
    T &operator*() { return *Ptr(); }
    const T &operator*() const { return *Ptr(); }
    T *operator->() { return Ptr(); }
    const T *operator->() const { return Ptr(); }
    T &Value() {
        if (!present_)
            throw BadOptionalAccess();
        return *Ptr();
    }
    const T &Value() const {
        if (!present_)
            throw BadOptionalAccess();
        return *Ptr();
    }
    void Reset() noexcept {
        if (present_) {
            Ptr()->~T();
            present_ = false;
        }
    }
    template <class... A> T &Emplace(A &&...args) {
        Reset();
        ::new (static_cast<void *>(bytes_)) T(std::forward<A>(args)...);
        present_ = true;
        return *Ptr();
    }
};

// Raw storage and object lifetime are separate. Reallocation prefers copying
// when move construction may throw, matching the usual vector guarantee.
template <class T> class Vector {
    std::allocator<T> alloc_;
    T *data_ = nullptr;
    std::size_t size_ = 0, capacity_ = 0;
    static void Destroy(T *p, std::size_t n) noexcept {
        while (n)
            p[--n].~T();
    }
    T *Allocate(std::size_t n) {
        if (n > std::allocator_traits<std::allocator<T>>::max_size(alloc_))
            throw std::length_error("Vector capacity");
        return n ? alloc_.allocate(n) : nullptr;
    }
    void Release(T *p, std::size_t n) noexcept {
        if (p)
            alloc_.deallocate(p, n);
    }
    std::size_t Offset(const T *p) const {
        if (!data_) {
            if (p)
                throw std::out_of_range("Vector iterator");
            return 0;
        }
        return static_cast<std::size_t>(p - data_); // requires an iterator of this vector
    }
    template <class... A> void GrowAndEmplace(std::size_t index, A &&...args) {
        if (size_ == std::numeric_limits<std::size_t>::max())
            throw std::length_error("Vector size");
        const auto n = capacity_ > std::numeric_limits<std::size_t>::max() / 2
                           ? size_ + 1
                           : std::max<std::size_t>(1, capacity_ * 2);
        T *fresh = Allocate(n);
        std::size_t left = 0, right = 0;
        bool inserted = false;
        try {
            // Construct first: arguments may refer to elements of the old buffer.
            ::new (static_cast<void *>(fresh + index)) T(std::forward<A>(args)...);
            inserted = true;
            for (; left < index; ++left)
                ::new (static_cast<void *>(fresh + left)) T(std::move_if_noexcept(data_[left]));
            for (; right < size_ - index; ++right)
                ::new (static_cast<void *>(fresh + index + 1 + right))
                    T(std::move_if_noexcept(data_[index + right]));
        } catch (...) {
            Destroy(fresh, left);
            Destroy(fresh + index + 1, right);
            if (inserted)
                fresh[index].~T();
            Release(fresh, n);
            throw;
        }
        Destroy(data_, size_);
        Release(data_, capacity_);
        data_ = fresh;
        capacity_ = n;
        ++size_;
    }

  public:
    using iterator = T *;
    using const_iterator = const T *;
    Vector() = default;
    explicit Vector(std::size_t n) : data_(Allocate(n)), capacity_(n) {
        try {
            for (; size_ < n; ++size_)
                ::new (static_cast<void *>(data_ + size_)) T();
        } catch (...) {
            Destroy(data_, size_);
            Release(data_, capacity_);
            throw;
        }
    }
    Vector(const Vector &x) : data_(Allocate(x.size_)), capacity_(x.size_) {
        try {
            for (; size_ < x.size_; ++size_)
                ::new (static_cast<void *>(data_ + size_)) T(x[size_]);
        } catch (...) {
            Destroy(data_, size_);
            Release(data_, capacity_);
            throw;
        }
    }
    Vector(Vector &&x) noexcept { Swap(x); }
    ~Vector() {
        Destroy(data_, size_);
        Release(data_, capacity_);
    }
    Vector &operator=(const Vector &x) {
        if (this != &x) {
            Vector copy(x);
            Swap(copy);
        }
        return *this;
    }
    Vector &operator=(Vector &&x) noexcept {
        if (this != &x) {
            Vector moved(std::move(x));
            Swap(moved);
        }
        return *this;
    }
    void Swap(Vector &x) noexcept {
        std::swap(data_, x.data_);
        std::swap(size_, x.size_);
        std::swap(capacity_, x.capacity_);
    }
    std::size_t Size() const noexcept { return size_; }
    std::size_t Capacity() const noexcept { return capacity_; }
    T &operator[](std::size_t i) { return data_[i]; }
    const T &operator[](std::size_t i) const { return data_[i]; }
    iterator begin() noexcept { return data_; }
    const_iterator begin() const noexcept { return data_; }
    iterator end() noexcept { return size_ ? data_ + size_ : data_; }
    const_iterator end() const noexcept { return size_ ? data_ + size_ : data_; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    void Reserve(std::size_t n) {
        if (n <= capacity_)
            return;
        T *fresh = Allocate(n);
        std::size_t done = 0;
        try {
            for (; done < size_; ++done)
                ::new (static_cast<void *>(fresh + done)) T(std::move_if_noexcept(data_[done]));
        } catch (...) {
            Destroy(fresh, done);
            Release(fresh, n);
            throw;
        }
        Destroy(data_, size_);
        Release(data_, capacity_);
        data_ = fresh;
        capacity_ = n;
    }
    template <class... A> iterator Emplace(const_iterator position, A &&...args) {
        const auto index = Offset(position);
        if (index > size_)
            throw std::out_of_range("Vector insertion");
        if (size_ == capacity_) {
            GrowAndEmplace(index, std::forward<A>(args)...);
        } else if (index == size_) {
            ::new (static_cast<void *>(data_ + size_)) T(std::forward<A>(args)...);
            ++size_;
        } else {
            T temporary(std::forward<A>(args)...);
            ::new (static_cast<void *>(data_ + size_)) T(std::move_if_noexcept(data_[size_ - 1]));
            ++size_; // maintain lifetime accounting even if a throwing move assignment fails
            std::move_backward(data_ + index, data_ + size_ - 2, data_ + size_ - 1);
            data_[index] = std::move(temporary);
        }
        return data_ + index;
    }
    template <class... A> T &EmplaceBack(A &&...args) {
        if (size_ == capacity_)
            GrowAndEmplace(size_, std::forward<A>(args)...);
        else {
            ::new (static_cast<void *>(data_ + size_)) T(std::forward<A>(args)...);
            ++size_;
        }
        return data_[size_ - 1];
    }
    void PushBack(const T &x) { EmplaceBack(x); }
    void PushBack(T &&x) { EmplaceBack(std::move(x)); }
    iterator Insert(const_iterator p, const T &x) { return Emplace(p, x); }
    iterator Insert(const_iterator p, T &&x) { return Emplace(p, std::move(x)); }
    iterator Erase(const_iterator p) {
        const auto i = Offset(p);
        if (i >= size_)
            throw std::out_of_range("Vector erase");
        std::move(data_ + i + 1, data_ + size_, data_ + i);
        PopBack();
        return data_ + i;
    }
    void PopBack() {
        if (!size_)
            throw std::out_of_range("Vector pop");
        data_[--size_].~T();
    }
    void Resize(std::size_t n) {
        if (n < size_) {
            while (size_ > n)
                PopBack();
            return;
        }
        if (n > capacity_)
            Reserve(n);
        const auto before = size_;
        try {
            while (size_ < n) {
                ::new (static_cast<void *>(data_ + size_)) T();
                ++size_;
            }
        } catch (...) {
            while (size_ > before)
                PopBack();
            throw;
        }
    }
};

struct Nucleotide {
    char Symbol;
    std::size_t Position;
    int ChromosomeNum;
    int GeneNum;
    bool IsMarked;
    char ServiceInfo;
};
struct CompactNucleotide {
    std::uint64_t bits = 0;
};
static_assert(sizeof(CompactNucleotide) <= 8);
inline CompactNucleotide Compress(const Nucleotide &n) {
    const auto symbol = std::string("ATGC").find(n.Symbol);
    if (symbol == std::string::npos || n.Position > 3300000000ULL || n.ChromosomeNum < 1 ||
        n.ChromosomeNum > 46 || n.GeneNum < 0 || n.GeneNum > 25000)
        throw std::invalid_argument("nucleotide range");
    return {static_cast<std::uint64_t>(n.Position) | (static_cast<std::uint64_t>(symbol) << 32) |
            (std::uint64_t(n.ChromosomeNum) << 34) | (std::uint64_t(n.GeneNum) << 40) |
            (std::uint64_t(n.IsMarked) << 55) |
            (std::uint64_t(static_cast<unsigned char>(n.ServiceInfo)) << 56)};
}
inline Nucleotide Decompress(CompactNucleotide n) {
    return {"ATGC"[(n.bits >> 32) & 3], std::size_t(n.bits & 0xffffffffULL),
            int((n.bits >> 34) & 63),   int((n.bits >> 40) & 32767),
            bool((n.bits >> 55) & 1),   static_cast<char>(n.bits >> 56)};
}

// This exercise deliberately uses the host's arithmetic representation and
// size_t. It is a same-ABI teaching format, not a portable storage protocol.
namespace binary {
inline void Read(std::istream &in, char *data, std::size_t count) {
    if (count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) ||
        !in.read(data, static_cast<std::streamsize>(count)))
        throw std::runtime_error("truncated binary value");
}
template <class T, class = void> struct Codec;
template <class T> struct Codec<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static void Write(const T &x, std::ostream &out) {
        out.write(reinterpret_cast<const char *>(&x), sizeof x);
        if (!out)
            throw std::runtime_error("binary write failed");
    }
    static T ReadValue(std::istream &in) {
        if constexpr (std::is_same_v<T, bool>) {
            unsigned char b;
            Read(in, reinterpret_cast<char *>(&b), 1);
            if (b > 1)
                throw std::runtime_error("invalid boolean");
            return b != 0;
        } else {
            T x;
            Read(in, reinterpret_cast<char *>(&x), sizeof x);
            return x;
        }
    }
};
inline std::size_t Count(std::istream &in) {
    auto n = Codec<std::size_t>::ReadValue(in);
    if (n > 10000000)
        throw std::length_error("binary container limit");
    return n;
}
template <> struct Codec<std::string> {
    static void Write(const std::string &x, std::ostream &out) {
        Codec<std::size_t>::Write(x.size(), out);
        out.write(x.data(), static_cast<std::streamsize>(x.size()));
        if (!out)
            throw std::runtime_error("binary write failed");
    }
    static std::string ReadValue(std::istream &in) {
        std::string s(Count(in), '\0');
        Read(in, s.data(), s.size());
        return s;
    }
};
template <class T> struct Codec<std::vector<T>> {
    static void Write(const std::vector<T> &x, std::ostream &out) {
        Codec<std::size_t>::Write(x.size(), out);
        for (const auto &e : x)
            Codec<T>::Write(e, out);
    }
    static std::vector<T> ReadValue(std::istream &in) {
        std::vector<T> x;
        const auto n = Count(in);
        x.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            x.push_back(Codec<T>::ReadValue(in));
        return x;
    }
};
template <class K, class V> struct Codec<std::map<K, V>> {
    static void Write(const std::map<K, V> &x, std::ostream &out) {
        Codec<std::size_t>::Write(x.size(), out);
        for (const auto &[k, v] : x) {
            Codec<K>::Write(k, out);
            Codec<V>::Write(v, out);
        }
    }
    static std::map<K, V> ReadValue(std::istream &in) {
        std::map<K, V> x;
        const auto n = Count(in);
        for (std::size_t i = 0; i < n; ++i) {
            auto k = Codec<K>::ReadValue(in);
            auto v = Codec<V>::ReadValue(in);
            if (!x.emplace(std::move(k), std::move(v)).second)
                throw std::runtime_error("duplicate map key");
        }
        return x;
    }
};
} // namespace binary
template <class T> void Serialize(const T &x, std::ostream &out) {
    binary::Codec<T>::Write(x, out);
}
template <class T> void Deserialize(std::istream &in, T &x) {
    T temporary = binary::Codec<T>::ReadValue(in);
    x = std::move(temporary);
}
} // namespace museum::black
