#pragma once
#include <algorithm>
#include <cstdint>
#include <istream>
#include <iterator>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace museum::black {
// A deliberately small protobuf wire implementation for contact.proto. Unknown
// ordinary wire fields are skipped. No protoc or protobuf C++ runtime is needed.
namespace wire {
inline void Varint(std::uint64_t x, std::string &out) {
    while (x >= 128) {
        out += static_cast<char>((x & 127) | 128);
        x >>= 7;
    }
    out += static_cast<char>(x);
}
inline void Bytes(unsigned field, std::string_view x, std::string &out) {
    Varint((std::uint64_t(field) << 3) | 2, out);
    Varint(x.size(), out);
    out.append(x);
}
inline void Integer(unsigned field, int x, std::string &out) {
    if (x) {
        Varint(std::uint64_t(field) << 3, out);
        Varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(x)), out);
    }
}
class Reader {
    std::string_view data_;
    std::size_t pos_ = 0;

  public:
    explicit Reader(std::string_view data) : data_(data) {}
    bool Done() const { return pos_ == data_.size(); }
    std::uint64_t Varint() {
        std::uint64_t result = 0;
        for (unsigned i = 0; i < 10; ++i) {
            if (Done())
                throw std::invalid_argument("truncated protobuf varint");
            const auto c = static_cast<unsigned char>(data_[pos_++]);
            if (i == 9 && c > 1)
                throw std::invalid_argument("protobuf varint overflow");
            result |= std::uint64_t(c & 127) << (7 * i);
            if (!(c & 128))
                return result;
        }
        throw std::invalid_argument("protobuf varint overflow");
    }
    std::uint64_t Tag() {
        auto tag = Varint();
        if ((tag >> 3) == 0 || (tag >> 3) > 536870911)
            throw std::invalid_argument("protobuf field");
        return tag;
    }
    std::string_view Bytes() {
        const auto n = Varint();
        if (n > data_.size() - pos_)
            throw std::invalid_argument("truncated protobuf bytes");
        auto out = data_.substr(pos_, static_cast<std::size_t>(n));
        pos_ += static_cast<std::size_t>(n);
        return out;
    }
    void Skip(unsigned type) {
        if (type == 0) {
            Varint();
            return;
        }
        if (type == 2) {
            Bytes();
            return;
        }
        const std::size_t n = type == 1 ? 8 : type == 5 ? 4 : 0;
        if (!n || n > data_.size() - pos_)
            throw std::invalid_argument("unsupported or truncated protobuf field");
        pos_ += n;
    }
};
} // namespace wire
struct Date {
    int year = 0, month = 0, day = 0;
};
struct Contact {
    std::string name;
    std::optional<Date> birthday;
    std::vector<std::string> phones;
};
template <class Iterator> class IteratorRange {
    Iterator first_, last_;

  public:
    IteratorRange(Iterator first, Iterator last) : first_(first), last_(last) {}
    Iterator begin() const { return first_; }
    Iterator end() const { return last_; }
    std::size_t size() const { return static_cast<std::size_t>(last_ - first_); }
};
class PhoneBook {
    std::vector<Contact> contacts_;

  public:
    explicit PhoneBook(std::vector<Contact> contacts) : contacts_(std::move(contacts)) {
        std::stable_sort(contacts_.begin(), contacts_.end(),
                         [](const auto &a, const auto &b) { return a.name < b.name; });
    }
    auto FindByNamePrefix(std::string_view prefix) const {
        const auto first = std::lower_bound(
            contacts_.begin(), contacts_.end(), prefix, [](const Contact &c, std::string_view p) {
                return std::string_view(c.name).compare(0, p.size(), p) < 0;
            });
        const auto last = std::upper_bound(
            first, contacts_.end(), prefix, [](std::string_view p, const Contact &c) {
                return std::string_view(c.name).compare(0, p.size(), p) > 0;
            });
        return IteratorRange(first, last);
    }
    void SaveTo(std::ostream &out) const {
        std::string book;
        for (const auto &c : contacts_) {
            std::string item;
            if (!c.name.empty())
                wire::Bytes(1, c.name, item);
            if (c.birthday) {
                std::string date;
                wire::Integer(1, c.birthday->year, date);
                wire::Integer(2, c.birthday->month, date);
                wire::Integer(3, c.birthday->day, date);
                wire::Bytes(2, date, item);
            }
            for (const auto &p : c.phones)
                wire::Bytes(3, p, item);
            wire::Bytes(1, item, book);
        }
        out.write(book.data(), static_cast<std::streamsize>(book.size()));
        if (!out)
            throw std::runtime_error("phonebook output failed");
    }
};
inline PhoneBook DeserializePhoneBook(std::istream &in) {
    std::string raw;
    char buffer[8192];
    while (in.read(buffer, sizeof buffer) || in.gcount()) {
        raw.append(buffer, static_cast<std::size_t>(in.gcount()));
        if (raw.size() > 64 * 1024 * 1024)
            throw std::length_error("phonebook input exceeds 64 MiB");
    }
    if (in.bad())
        throw std::runtime_error("phonebook input failed");
    wire::Reader book(raw);
    std::vector<Contact> contacts;
    while (!book.Done()) {
        const auto tag = book.Tag();
        if (tag != 10) {
            book.Skip(tag & 7);
            continue;
        }
        wire::Reader item(book.Bytes());
        Contact c;
        while (!item.Done()) {
            const auto field = item.Tag();
            if (field == 10)
                c.name = std::string(item.Bytes());
            else if (field == 26)
                c.phones.emplace_back(item.Bytes());
            else if (field == 18) {
                if (!c.birthday)
                    c.birthday = Date{};
                wire::Reader date(item.Bytes());
                while (!date.Done()) {
                    const auto part = date.Tag();
                    if (part == 8 || part == 16 || part == 24) {
                        const auto bits = static_cast<std::uint32_t>(date.Varint());
                        const auto value = bits <= 2147483647U ? static_cast<int>(bits)
                                                               : -1 - static_cast<int>(~bits);
                        if (part == 8)
                            c.birthday->year = value;
                        else if (part == 16)
                            c.birthday->month = value;
                        else
                            c.birthday->day = value;
                    } else
                        date.Skip(part & 7);
                }
            } else
                item.Skip(field & 7);
        }
        contacts.push_back(std::move(c));
    }
    return PhoneBook(std::move(contacts));
}
} // namespace museum::black
