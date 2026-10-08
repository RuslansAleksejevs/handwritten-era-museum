#pragma once
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace museum::black {
inline void JsonRaw(std::ostream &out, std::string_view s) {
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
}
inline void PrintJsonString(std::ostream &out, std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    out.put('"');
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out.put('\\');
            out.put(static_cast<char>(c));
        } else if (c < 32) {
            JsonRaw(out, "\\u00");
            out.put(hex[c >> 4]);
            out.put(hex[c & 15]);
        } else
            out.put(static_cast<char>(c));
    }
    out.put('"');
}
namespace json_print {
struct State {
    struct Frame {
        char kind;
        bool first = true, value_pending = false;
    };
    std::ostream &out;
    std::vector<Frame> stack;
    explicit State(std::ostream &stream) : out(stream) {}
    void Value() {
        if (stack.empty())
            return;
        auto &f = stack.back();
        if (f.kind == '[') {
            if (!f.first)
                out.put(',');
            f.first = false;
        } else {
            if (!f.value_pending)
                throw std::logic_error("JSON key required");
            f.value_pending = false;
        }
    }
    void Key(std::string_view key) {
        auto &f = stack.back();
        if (f.kind != '{' || f.value_pending)
            throw std::logic_error("JSON key state");
        if (!f.first)
            out.put(',');
        f.first = false;
        PrintJsonString(out, key);
        out.put(':');
        f.value_pending = true;
    }
    void Begin(char kind) {
        Value();
        out.put(kind);
        stack.push_back({kind});
    }
    void End(char kind) {
        if (stack.empty() || stack.back().kind != kind)
            throw std::logic_error("JSON closing state");
        if (stack.back().value_pending)
            JsonRaw(out, "null");
        out.put(kind == '[' ? ']' : '}');
        stack.pop_back();
    }
    ~State() {
        try {
            while (!stack.empty())
                End(stack.back().kind);
        } catch (...) {
        }
    }
};
struct Done {};
template <class P> class Array;
template <class P> class Object;
template <class P> class Value {
    std::shared_ptr<State> state_;
    P parent_;

  public:
    Value(std::shared_ptr<State> state, P parent)
        : state_(std::move(state)), parent_(std::move(parent)) {}
    P Number(std::int64_t x) {
        state_->Value();
        JsonRaw(state_->out, std::to_string(x));
        return parent_;
    }
    P String(std::string_view x) {
        state_->Value();
        PrintJsonString(state_->out, x);
        return parent_;
    }
    P Boolean(bool x) {
        state_->Value();
        JsonRaw(state_->out, x ? "true" : "false");
        return parent_;
    }
    P Null() {
        state_->Value();
        JsonRaw(state_->out, "null");
        return parent_;
    }
    Array<P> BeginArray() {
        state_->Begin('[');
        return {state_, parent_};
    }
    Object<P> BeginObject() {
        state_->Begin('{');
        return {state_, parent_};
    }
};
template <class P> class Array {
    std::shared_ptr<State> state_;
    P parent_;

  public:
    Array(std::shared_ptr<State> state, P parent)
        : state_(std::move(state)), parent_(std::move(parent)) {}
    Array Number(std::int64_t x) { return Value<Array>(state_, *this).Number(x); }
    Array String(std::string_view x) { return Value<Array>(state_, *this).String(x); }
    Array Boolean(bool x) { return Value<Array>(state_, *this).Boolean(x); }
    Array Null() { return Value<Array>(state_, *this).Null(); }
    Array<Array> BeginArray() {
        state_->Begin('[');
        return {state_, *this};
    }
    Object<Array> BeginObject() {
        state_->Begin('{');
        return {state_, *this};
    }
    P EndArray() {
        state_->End('[');
        return parent_;
    }
};
template <class P> class Object {
    std::shared_ptr<State> state_;
    P parent_;

  public:
    Object(std::shared_ptr<State> state, P parent)
        : state_(std::move(state)), parent_(std::move(parent)) {}
    Value<Object> Key(std::string_view x) {
        state_->Key(x);
        return {state_, *this};
    }
    P EndObject() {
        state_->End('{');
        return parent_;
    }
};
} // namespace json_print
inline json_print::Array<json_print::Done> PrintJsonArray(std::ostream &out) {
    auto s = std::make_shared<json_print::State>(out);
    s->Begin('[');
    return {s, {}};
}
inline json_print::Object<json_print::Done> PrintJsonObject(std::ostream &out) {
    auto s = std::make_shared<json_print::State>(out);
    s->Begin('{');
    return {s, {}};
}
} // namespace museum::black
