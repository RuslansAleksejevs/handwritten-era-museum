#include "rectangle.h"
namespace yellow {
Rectangle::Rectangle(int w, int h) : width_(w), height_(h) {}
int Rectangle::GetArea() const { return width_ * height_; }
int Rectangle::GetPerimeter() const { return 2 * (width_ + height_); }
int Rectangle::GetWidth() const { return width_; }
int Rectangle::GetHeight() const { return height_; }
} // namespace yellow
