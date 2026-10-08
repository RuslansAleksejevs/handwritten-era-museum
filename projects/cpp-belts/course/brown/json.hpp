#pragma once
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <istream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace museum::brown::json {
class Node {
  public:
    using Array = std::vector<Node>;
    using Object = std::map<std::string, Node>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

  private:
    Value value_ = nullptr;

  public:
    Node() = default;
    Node(std::nullptr_t) : value_(nullptr) {}
    Node(bool v) : value_(v) {}
    Node(double v) : value_(v) {
        if (!std::isfinite(v))
            throw std::invalid_argument("JSON number must be finite");
    }
    template <class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    Node(T v) : Node(static_cast<double>(v)) {}
    Node(std::string v) : value_(std::move(v)) {}
    Node(const char *v) : value_(std::string(v)) {}
    Node(Array v) : value_(std::move(v)) {}
    Node(Object v) : value_(std::move(v)) {}
    bool IsNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool IsObject() const { return std::holds_alternative<Object>(value_); }
    bool IsArray() const { return std::holds_alternative<Array>(value_); }
    bool IsString() const { return std::holds_alternative<std::string>(value_); }
    bool IsNumber() const { return std::holds_alternative<double>(value_); }
    bool IsBool() const { return std::holds_alternative<bool>(value_); }
    const Object &AsObject() const { return std::get<Object>(value_); }
    Object &AsObject() { return std::get<Object>(value_); }
    const Array &AsArray() const { return std::get<Array>(value_); }
    Array &AsArray() { return std::get<Array>(value_); }
    const std::string &AsString() const { return std::get<std::string>(value_); }
    double AsNumber() const { return std::get<double>(value_); }
    bool AsBool() const { return std::get<bool>(value_); }
    int AsInt() const {
        double n = AsNumber();
        if (n < std::numeric_limits<int>::min() || n > std::numeric_limits<int>::max() ||
            std::trunc(n) != n)
            throw std::out_of_range("JSON integer outside int range");
        return static_cast<int>(n);
    }
    bool Contains(const std::string &key) const { return AsObject().count(key); }
    const Node &At(const std::string &key) const { return AsObject().at(key); }
    Node &At(const std::string &key) { return AsObject().at(key); }
    const Value &GetValue() const { return value_; }
};
using Array = Node::Array;
using Object = Node::Object;
namespace detail {
class Parser {
    std::string_view text_;
    std::size_t pos_ = 0;
    [[noreturn]] void Error() const {
        throw std::invalid_argument("invalid JSON at byte " + std::to_string(pos_));
    }
    void Space() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\n' ||
                                       text_[pos_] == '\r' || text_[pos_] == '\t'))
            ++pos_;
    }
    char Take() {
        if (pos_ == text_.size())
            Error();
        return text_[pos_++];
    }
    bool Consume(char c) {
        Space();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }
    unsigned Hex() {
        unsigned n = 0;
        for (int i = 0; i < 4; ++i) {
            char c = Take();
            n *= 16;
            if (c >= '0' && c <= '9')
                n += c - '0';
            else if (c >= 'a' && c <= 'f')
                n += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                n += c - 'A' + 10;
            else
                Error();
        }
        return n;
    }
    static void Utf8(std::string &out, unsigned n) {
        if (n <= 0x7f)
            out += static_cast<char>(n);
        else if (n <= 0x7ff) {
            out += static_cast<char>(0xc0 | (n >> 6));
            out += static_cast<char>(0x80 | (n & 63));
        } else if (n <= 0xffff) {
            out += static_cast<char>(0xe0 | (n >> 12));
            out += static_cast<char>(0x80 | ((n >> 6) & 63));
            out += static_cast<char>(0x80 | (n & 63));
        } else {
            out += static_cast<char>(0xf0 | (n >> 18));
            out += static_cast<char>(0x80 | ((n >> 12) & 63));
            out += static_cast<char>(0x80 | ((n >> 6) & 63));
            out += static_cast<char>(0x80 | (n & 63));
        }
    }
    std::string String() {
        if (Take() != '"')
            Error();
        std::string out;
        for (;;) {
            unsigned char c = Take();
            if (c == '"')
                return out;
            if (c < 32)
                Error();
            if (c != '\\') {
                out += static_cast<char>(c);
                continue;
            }
            switch (Take()) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                unsigned n = Hex();
                if (n >= 0xd800 && n <= 0xdbff) {
                    if (Take() != '\\' || Take() != 'u')
                        Error();
                    auto low = Hex();
                    if (low < 0xdc00 || low > 0xdfff)
                        Error();
                    n = 0x10000 + ((n - 0xd800) << 10) + (low - 0xdc00);
                } else if (n >= 0xdc00 && n <= 0xdfff)
                    Error();
                Utf8(out, n);
                break;
            }
            default:
                Error();
            }
        }
    }
    Node Number() {
        const auto start = pos_;
        if (text_[pos_] == '-')
            ++pos_;
        if (pos_ == text_.size())
            Error();
        if (text_[pos_] == '0')
            ++pos_;
        else {
            if (text_[pos_] < '1' || text_[pos_] > '9')
                Error();
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
                ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            auto first = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
                ++pos_;
            if (first == pos_)
                Error();
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
                ++pos_;
            auto first = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9')
                ++pos_;
            if (first == pos_)
                Error();
        }
        std::istringstream in(std::string(text_.substr(start, pos_ - start)));
        in.imbue(std::locale::classic());
        double value;
        in >> value;
        if (!in || !std::isfinite(value))
            Error();
        return value;
    }
    Node Value(unsigned depth) {
        Space();
        if (depth > 256 || pos_ == text_.size())
            Error();
        char c = text_[pos_];
        if (c == '"')
            return String();
        if (c == '[') {
            ++pos_;
            Array out;
            if (Consume(']'))
                return out;
            do {
                out.push_back(Value(depth + 1));
            } while (Consume(','));
            if (!Consume(']'))
                Error();
            return out;
        }
        if (c == '{') {
            ++pos_;
            Object out;
            if (Consume('}'))
                return out;
            do {
                Space();
                auto key = String();
                if (!Consume(':'))
                    Error();
                if (!out.emplace(std::move(key), Value(depth + 1)).second)
                    Error();
            } while (Consume(','));
            if (!Consume('}'))
                Error();
            return out;
        }
        for (const auto &[word, node] : std::vector<std::pair<std::string_view, Node>>{
                 {"null", nullptr}, {"true", true}, {"false", false}}) {
            if (text_.substr(pos_, word.size()) == word) {
                pos_ += word.size();
                return node;
            }
        }
        return Number();
    }

  public:
    explicit Parser(std::string_view text) : text_(text) {}
    Node Parse() {
        auto out = Value(0);
        Space();
        if (pos_ != text_.size())
            Error();
        return out;
    }
};
inline void Quote(std::ostream &out, const std::string &s) {
    static constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 32)
                out << "\\u00" << hex[c >> 4] << hex[c & 15];
            else
                out << c;
        }
    }
    out << '"';
}
} // namespace detail
inline Node Parse(std::string_view text) { return detail::Parser(text).Parse(); }
inline Node Load(std::istream &in) {
    std::string text{std::istreambuf_iterator<char>(in), {}};
    if (in.bad())
        throw std::runtime_error("JSON input failed");
    return Parse(text);
}
inline void Serialize(const Node &node, std::ostream &out) {
    if (node.IsNull())
        out << "null";
    else if (node.IsBool())
        out << (node.AsBool() ? "true" : "false");
    else if (node.IsNumber()) {
        std::ostringstream number;
        number.imbue(std::locale::classic());
        number << std::setprecision(17) << node.AsNumber();
        out << number.str();
    } else if (node.IsString())
        detail::Quote(out, node.AsString());
    else if (node.IsArray()) {
        out << '[';
        bool first = true;
        for (const auto &value : node.AsArray()) {
            if (!first)
                out << ',';
            first = false;
            Serialize(value, out);
        }
        out << ']';
    } else {
        out << '{';
        bool first = true;
        for (const auto &[key, value] : node.AsObject()) {
            if (!first)
                out << ',';
            first = false;
            detail::Quote(out, key);
            out << ':';
            Serialize(value, out);
        }
        out << '}';
    }
}
inline std::string Serialize(const Node &node) {
    std::ostringstream out;
    Serialize(node, out);
    return out.str();
}
} // namespace museum::brown::json
