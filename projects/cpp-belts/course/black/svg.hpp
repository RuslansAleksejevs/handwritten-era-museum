#pragma once
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Svg {
struct Point {
    double x = 0, y = 0;
};
struct Rgb {
    int red = 0, green = 0, blue = 0;
};
struct Rgba {
    int red = 0, green = 0, blue = 0;
    double opacity = 1;
};
class Color {
    std::variant<std::monostate, std::string, Rgb, Rgba> value_;

  public:
    Color() = default;
    Color(const char *x) : value_(std::string(x)) {}
    Color(std::string x) : value_(std::move(x)) {}
    Color(Rgb x) : value_(x) {}
    Color(Rgba x) : value_(x) {}
    std::string ToString() const {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(17);
        std::visit(
            [&](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                    out << "none";
                else if constexpr (std::is_same_v<T, std::string>)
                    out << v;
                else {
                    out << (std::is_same_v<T, Rgb> ? "rgb(" : "rgba(") << v.red << ',' << v.green
                        << ',' << v.blue;
                    if constexpr (std::is_same_v<T, Rgba>)
                        out << ',' << v.opacity;
                    out << ')';
                }
            },
            value_);
        return out.str();
    }
};
inline const Color NoneColor{};
inline std::string Escape(const std::string &s) {
    std::string out;
    for (char c : s)
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&apos;";
            break;
        default:
            out += c;
        }
    return out;
}
template <class D> class Style {
    Color fill_, stroke_;
    double width_ = 1;
    std::string cap_, join_;

  protected:
    void Attributes(std::ostream &out) const {
        out << " fill=\"" << Escape(fill_.ToString()) << "\" stroke=\""
            << Escape(stroke_.ToString()) << "\" stroke-width=\"" << width_ << '"';
        if (!cap_.empty())
            out << " stroke-linecap=\"" << Escape(cap_) << '"';
        if (!join_.empty())
            out << " stroke-linejoin=\"" << Escape(join_) << '"';
    }

  public:
    D &SetFillColor(const Color &c) {
        fill_ = c;
        return static_cast<D &>(*this);
    }
    D &SetStrokeColor(const Color &c) {
        stroke_ = c;
        return static_cast<D &>(*this);
    }
    D &SetStrokeWidth(double x) {
        width_ = x;
        return static_cast<D &>(*this);
    }
    D &SetStrokeLineCap(const std::string &x) {
        cap_ = x;
        return static_cast<D &>(*this);
    }
    D &SetStrokeLineJoin(const std::string &x) {
        join_ = x;
        return static_cast<D &>(*this);
    }
};
class Circle : public Style<Circle> {
    Point center_;
    double radius_ = 1;

  public:
    Circle &SetCenter(Point p) {
        center_ = p;
        return *this;
    }
    Circle &SetRadius(double r) {
        radius_ = r;
        return *this;
    }
    void Render(std::ostream &out) const {
        out << "<circle cx=\"" << center_.x << "\" cy=\"" << center_.y << "\" r=\"" << radius_
            << '"';
        Attributes(out);
        out << " />";
    }
};
class Polyline : public Style<Polyline> {
    std::vector<Point> points_;

  public:
    Polyline &AddPoint(Point p) {
        points_.push_back(p);
        return *this;
    }
    void Render(std::ostream &out) const {
        out << "<polyline points=\"";
        bool first = true;
        for (auto p : points_) {
            if (!first)
                out << ' ';
            first = false;
            out << p.x << ',' << p.y;
        }
        out << '"';
        Attributes(out);
        out << " />";
    }
};
class Text : public Style<Text> {
    Point point_, offset_;
    std::uint32_t size_ = 1;
    std::string family_, weight_, data_;

  public:
    Text &SetPoint(Point x) {
        point_ = x;
        return *this;
    }
    Text &SetOffset(Point x) {
        offset_ = x;
        return *this;
    }
    Text &SetFontSize(std::uint32_t x) {
        size_ = x;
        return *this;
    }
    Text &SetFontFamily(const std::string &x) {
        family_ = x;
        return *this;
    }
    Text &SetFontWeight(const std::string &x) {
        weight_ = x;
        return *this;
    }
    Text &SetData(const std::string &x) {
        data_ = x;
        return *this;
    }
    void Render(std::ostream &out) const {
        out << "<text x=\"" << point_.x << "\" y=\"" << point_.y << "\" dx=\"" << offset_.x
            << "\" dy=\"" << offset_.y << "\" font-size=\"" << size_ << '"';
        if (!family_.empty())
            out << " font-family=\"" << Escape(family_) << '"';
        if (!weight_.empty())
            out << " font-weight=\"" << Escape(weight_) << '"';
        Attributes(out);
        out << '>' << Escape(data_) << "</text>";
    }
};
class Document {
    std::vector<std::variant<Circle, Polyline, Text>> objects_;
    double width_ = 0, height_ = 0;

  public:
    void SetSize(double width, double height) {
        width_ = width;
        height_ = height;
    }
    template <class T> void Add(T object) { objects_.emplace_back(std::move(object)); }
    void Render(std::ostream &output) const {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(17);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?><svg "
               "xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"";
        if (width_ > 0 && height_ > 0)
            out << " width=\"" << width_ << "\" height=\"" << height_ << "\" viewBox=\"0 0 "
                << width_ << ' ' << height_ << "\"";
        out << '>';
        for (const auto &x : objects_)
            std::visit([&](const auto &v) { v.Render(out); }, x);
        out << "</svg>";
        output << out.str();
    }
};
} // namespace Svg
