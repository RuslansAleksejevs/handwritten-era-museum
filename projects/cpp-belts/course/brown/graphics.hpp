#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace museum::brown::geometry {
struct Point {
    long double x, y;
};
struct Rectangle {
    Point left_top, right_bottom;
};
struct Circle {
    Point center;
    long double radius;
};
struct Segment {
    Point first, second;
};
inline long double Squared(Point a, Point b) {
    auto x = a.x - b.x, y = a.y - b.y;
    return x * x + y * y;
}
inline long double Cross(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
inline bool Collide(Point a, Point b) { return a.x == b.x && a.y == b.y; }
inline bool Collide(Point p, Segment s) {
    return Cross(s.first, s.second, p) == 0 && p.x >= std::min(s.first.x, s.second.x) &&
           p.x <= std::max(s.first.x, s.second.x) && p.y >= std::min(s.first.y, s.second.y) &&
           p.y <= std::max(s.first.y, s.second.y);
}
inline bool Collide(Point p, Rectangle r) {
    return p.x >= std::min(r.left_top.x, r.right_bottom.x) &&
           p.x <= std::max(r.left_top.x, r.right_bottom.x) &&
           p.y >= std::min(r.left_top.y, r.right_bottom.y) &&
           p.y <= std::max(r.left_top.y, r.right_bottom.y);
}
inline bool Collide(Point p, Circle c) { return Squared(p, c.center) <= c.radius * c.radius; }
inline bool Collide(Segment s, Point p) { return Collide(p, s); }
inline bool Collide(Rectangle r, Point p) { return Collide(p, r); }
inline bool Collide(Circle c, Point p) { return Collide(p, c); }
inline bool Collide(Segment a, Segment b) {
    const auto p = Cross(a.first, a.second, b.first), q = Cross(a.first, a.second, b.second),
               r = Cross(b.first, b.second, a.first), s = Cross(b.first, b.second, a.second);
    if (((p < 0 && q > 0) || (p > 0 && q < 0)) && ((r < 0 && s > 0) || (r > 0 && s < 0)))
        return true;
    return Collide(a.first, b) || Collide(a.second, b) || Collide(b.first, a) ||
           Collide(b.second, a);
}
inline bool Collide(Circle a, Circle b) {
    return Squared(a.center, b.center) <= (a.radius + b.radius) * (a.radius + b.radius);
}
inline bool Collide(Circle c, Segment s) {
    const auto length = Squared(s.first, s.second);
    if (length == 0)
        return Collide(s.first, c);
    auto t = ((c.center.x - s.first.x) * (s.second.x - s.first.x) +
              (c.center.y - s.first.y) * (s.second.y - s.first.y)) /
             length;
    t = std::clamp(t, 0.0L, 1.0L);
    return Collide(
        Point{s.first.x + t * (s.second.x - s.first.x), s.first.y + t * (s.second.y - s.first.y)},
        c);
}
inline bool Collide(Segment s, Circle c) { return Collide(c, s); }
inline bool Collide(Circle c, Rectangle r) {
    return Collide(Point{std::clamp(c.center.x, std::min(r.left_top.x, r.right_bottom.x),
                                    std::max(r.left_top.x, r.right_bottom.x)),
                         std::clamp(c.center.y, std::min(r.left_top.y, r.right_bottom.y),
                                    std::max(r.left_top.y, r.right_bottom.y))},
                   c);
}
inline bool Collide(Rectangle r, Circle c) { return Collide(c, r); }
inline bool Collide(Rectangle a, Rectangle b) {
    return std::max(std::min(a.left_top.x, a.right_bottom.x),
                    std::min(b.left_top.x, b.right_bottom.x)) <=
               std::min(std::max(a.left_top.x, a.right_bottom.x),
                        std::max(b.left_top.x, b.right_bottom.x)) &&
           std::max(std::min(a.left_top.y, a.right_bottom.y),
                    std::min(b.left_top.y, b.right_bottom.y)) <=
               std::min(std::max(a.left_top.y, a.right_bottom.y),
                        std::max(b.left_top.y, b.right_bottom.y));
}
inline bool Collide(Rectangle r, Segment s) {
    Point a = r.left_top, b{r.right_bottom.x, r.left_top.y}, c = r.right_bottom,
          d{r.left_top.x, r.right_bottom.y};
    return Collide(s.first, r) || Collide(s.second, r) || Collide(s, Segment{a, b}) ||
           Collide(s, Segment{b, c}) || Collide(s, Segment{c, d}) || Collide(s, Segment{d, a});
}
inline bool Collide(Segment s, Rectangle r) { return Collide(r, s); }
class Unit;
class Building;
class Tower;
class Fence;
struct GameObject {
    virtual ~GameObject() = default;
    virtual bool Collide(const GameObject &) const = 0;
    virtual bool CollideWith(const Unit &) const = 0;
    virtual bool CollideWith(const Building &) const = 0;
    virtual bool CollideWith(const Tower &) const = 0;
    virtual bool CollideWith(const Fence &) const = 0;
};
template <class Derived, class Shape> class Object : public GameObject {
    Shape shape_;

  public:
    explicit Object(Shape shape) : shape_(std::move(shape)) {}
    const Shape &GetShape() const { return shape_; }
    bool Collide(const GameObject &other) const override {
        return other.CollideWith(static_cast<const Derived &>(*this));
    }
    bool CollideWith(const Unit &) const override;
    bool CollideWith(const Building &) const override;
    bool CollideWith(const Tower &) const override;
    bool CollideWith(const Fence &) const override;
};
class Unit : public Object<Unit, Point> {
  public:
    using Object::Object;
};
class Building : public Object<Building, Rectangle> {
  public:
    using Object::Object;
};
class Tower : public Object<Tower, Circle> {
  public:
    using Object::Object;
};
class Fence : public Object<Fence, Segment> {
  public:
    using Object::Object;
};
template <class D, class S> bool Object<D, S>::CollideWith(const Unit &other) const {
    return geometry::Collide(shape_, other.GetShape());
}
template <class D, class S> bool Object<D, S>::CollideWith(const Building &other) const {
    return geometry::Collide(shape_, other.GetShape());
}
template <class D, class S> bool Object<D, S>::CollideWith(const Tower &other) const {
    return geometry::Collide(shape_, other.GetShape());
}
template <class D, class S> bool Object<D, S>::CollideWith(const Fence &other) const {
    return geometry::Collide(shape_, other.GetShape());
}
inline bool Collide(const GameObject &a, const GameObject &b) { return a.Collide(b); }
} // namespace museum::brown::geometry
namespace museum::brown::graphics {
using Image = std::vector<std::string>;
struct Point {
    int x = 0, y = 0;
};
struct Size {
    int width = 0, height = 0;
};
enum class ShapeType { Rectangle, Ellipse };
struct ITexture {
    virtual ~ITexture() = default;
    virtual Size GetSize() const = 0;
    virtual const Image &GetImage() const = 0;
};
struct IShape {
    virtual ~IShape() = default;
    virtual std::unique_ptr<IShape> Clone() const = 0;
    virtual void SetPosition(Point) = 0;
    virtual Point GetPosition() const = 0;
    virtual void SetSize(Size) = 0;
    virtual Size GetSize() const = 0;
    virtual void SetTexture(std::shared_ptr<ITexture>) = 0;
    virtual ITexture *GetTexture() const = 0;
    virtual void Draw(Image &) const = 0;
};
inline bool IsPointInEllipse(Point point, Size size) {
    if (size.width <= 0 || size.height <= 0)
        return false;
    const double x = (point.x + .5) * 2 / size.width - 1, y = (point.y + .5) * 2 / size.height - 1;
    return x * x + y * y <= 1;
}
class Shape : public IShape {
    ShapeType type_;
    Point position_;
    Size size_;
    std::shared_ptr<ITexture> texture_;

  public:
    explicit Shape(ShapeType type) : type_(type) {}
    std::unique_ptr<IShape> Clone() const override { return std::make_unique<Shape>(*this); }
    void SetPosition(Point p) override { position_ = p; }
    Point GetPosition() const override { return position_; }
    void SetSize(Size s) override { size_ = s; }
    Size GetSize() const override { return size_; }
    void SetTexture(std::shared_ptr<ITexture> t) override { texture_ = std::move(t); }
    ITexture *GetTexture() const override { return texture_.get(); }
    void Draw(Image &image) const override {
        for (std::size_t y = 0; y < image.size(); ++y)
            for (std::size_t x = 0; x < image[y].size(); ++x) {
                const auto local_x = static_cast<long long>(x) - position_.x,
                           local_y = static_cast<long long>(y) - position_.y;
                if (local_x < 0 || local_y < 0 || local_x >= size_.width || local_y >= size_.height)
                    continue;
                Point local{static_cast<int>(local_x), static_cast<int>(local_y)};
                if (type_ == ShapeType::Ellipse && !IsPointInEllipse(local, size_))
                    continue;
                char fill = '.';
                if (texture_) {
                    const auto &pixels = texture_->GetImage();
                    if (static_cast<std::size_t>(local.y) < pixels.size() &&
                        static_cast<std::size_t>(local.x) < pixels[local.y].size())
                        fill = pixels[local.y][local.x];
                }
                image[y][x] = fill;
            }
    }
};
inline std::unique_ptr<IShape> MakeShape(ShapeType type) { return std::make_unique<Shape>(type); }
} // namespace museum::brown::graphics
