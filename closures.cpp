#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr float kPi = 3.14159265358979323846f;

// Shapes
struct Shape {
  // Using public members for simplicity
  float x;
  float y;

  Shape(float x, float y) : x(x), y(y) {}

  virtual ~Shape() = default;

  virtual void move(float dx, float dy) {
    x += dx;
    y += dy;
  }

  virtual void scale(float factor) = 0;

  virtual void rotate(float rotation) = 0;

  virtual void print(std::ostream& os, const std::string& indent) const = 0;
};

inline std::ostream& operator<<(std::ostream& os, const Shape& shape) {
  shape.print(os, "");
  return os;
}

struct Circle : Shape {
  float radius;

  Circle(float x, float y, float radius) : Shape(x, y), radius(radius) {}

  void scale(float factor) override {
    radius *= factor;
  }

  void rotate(float angle_rad) override {
    (void) angle_rad;
  };

  void print(std::ostream& os, const std::string& indent) const override {
    os << indent << "Circle(x=" << x << ", y=" << y << ", radius=" << radius << ")\n";
  }
};

struct Rectangle : Shape {
  float width;
  float height;
  float orientation;

  Rectangle(float x, float y, float width, float height, float orientation = 0.0f)
      : Shape(x, y), width(width), height(height), orientation(orientation) {}

  void scale(float factor) override {
    width *= factor;
    height *= factor;
  }

  void rotate(float rotation) override {
    // Range is [-pi, pi]
    orientation = std::remainder(orientation + rotation, 2.0f * kPi);
  }

  void print(std::ostream& os, const std::string& indent) const override {
    os << indent << "Rectangle(x=" << x << ", y=" << y << ", width=" << width << ", height=" << height
       << ", orientation=" << orientation << ")\n";
  }
};

struct CompoundShape : Shape {
  std::vector<std::unique_ptr<Shape>> shapes;

  CompoundShape(float x, float y) : Shape(x, y) {}

  Shape* place(std::unique_ptr<Shape> shape) {
    shapes.push_back(std::move(shape));
    return shapes.back().get();
  }

  void scale(float factor) override {
    // Move and scale child shapes
    for (auto& shape : shapes) {
      const float dx = shape->x * (factor - 1.0f);
      const float dy = shape->y * (factor - 1.0f);
      shape->move(dx, dy);
      shape->scale(factor);
    }
  }

  void rotate(float rotation) override {
    // Move and scale child shapes
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);
    for (auto& shape : shapes) {
      const float rx = shape->x;
      const float ry = shape->y;
      const float nx = rx * c - ry * s;
      const float ny = rx * s + ry * c;
      shape->move(nx - rx, ny - ry);
      shape->rotate(rotation);
    }
  }

  void print(std::ostream& os, const std::string& indent) const override {
    os << indent << "CompoundShape(x=" << x << ", y=" << y << ") {\n";
    for (const auto& shape : shapes) {
      shape->print(os, indent + "  ");
    }
    os << indent << "}\n";
  }
};

// Canvas
class Canvas {

  using History = std::vector<std::pair<std::function<void()>, std::function<void()>>>;

  // The canvas owns the shapes
  std::vector<std::unique_ptr<Shape>> shapes_;

  History history_;
  History::iterator cursor_;

public:
  Canvas() : cursor_(history_.end()) {}

  Shape& place(std::unique_ptr<Shape> shape) {
    shapes_.push_back(std::move(shape));
    return *shapes_.back();
  }

  void move(Shape& shape, float dx, float dy) {
    shape.move(dx, dy);
    remember([&shape, dx, dy] { shape.move(dx, dy); },
             [&shape, dx, dy] { shape.move(-dx, -dy); });
  }

  void scale(Shape& shape, float factor) {
    shape.scale(factor);
    remember([&shape, factor] { shape.scale(factor); },
             [&shape, factor] { shape.scale(1.0f / factor); });
  }

  void rotate(Shape& shape, float angle_rad) {
    shape.rotate(angle_rad);
    remember([&shape, angle_rad] { shape.rotate(angle_rad); },
             [&shape, angle_rad] { shape.rotate(-angle_rad); });
  }

  void undo() {
    if (cursor_ == history_.begin()) return;
    --cursor_;
    auto& [apply, revert] = *cursor_;
    revert();
  }

  void redo() {
    if (cursor_ == history_.end()) return;
    auto& [apply, revert] = *cursor_;
    apply();
    ++cursor_;
  }

  friend std::ostream& operator<<(std::ostream& os, const Canvas& c) {
    for (const auto& shape : c.shapes_) {
      os << *shape;
    }
    return os;
  }

private:
  void remember(std::function<void()> apply, std::function<void()> revert) {
    history_.resize(cursor_ - history_.begin());
    history_.emplace_back(std::move(apply), std::move(revert));
    cursor_ = history_.end();
  }
};

void benchmark() {
  constexpr int kShapes = 1000;
  constexpr int kOps = 10000;

  Canvas canvas;
  std::vector<Shape*> shapes;
  shapes.reserve(kShapes);
  for (int i = 0; i < kShapes; ++i) {
    if (i % 2 == 0) {
      shapes.push_back(&canvas.place(std::make_unique<Circle>(float(i), float(i), 0.5f)));
    } else {
      shapes.push_back(&canvas.place(std::make_unique<Rectangle>(float(i), float(i), 2.0f, 1.0f)));
    }
  }

  using clk = std::chrono::steady_clock;
  auto t0 = clk::now();
  for (int i = 0; i < kOps; ++i) {
    Shape& s = *shapes[i % kShapes];
    switch (i % 3) {
      case 0:
        canvas.move(s, 0.1f, 0.1f);
        break;
      case 1:
        canvas.scale(s, 1.001f);
        break;
      case 2:
        canvas.rotate(s, 0.001f);
        break;
    }
  }
  auto t1 = clk::now();
  for (int i = 0; i < kOps; ++i) {
    canvas.undo();
  }
  auto t2 = clk::now();
  for (int i = 0; i < kOps; ++i) {
    canvas.redo();
  }
  auto t3 = clk::now();

  auto us = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count(); };
  std::cout << "[closures] apply=" << us(t0, t1) << "us undo=" << us(t1, t2) << "us redo=" << us(t2, t3) << "us\n";
}

int main() {
  Canvas canvas;
  auto& circle = canvas.place(std::make_unique<Circle>(1.0f, 0.0f, 0.5f));
  auto& rect = canvas.place(std::make_unique<Rectangle>(-1.0f, 0.0f, 2.0f, 1.0f));

  auto compound = std::make_unique<CompoundShape>(5.0f, 5.0f);
  compound->place(std::make_unique<Circle>(-1.0f, 0.0f, 1.0f));
  compound->place(std::make_unique<Rectangle>(1.0f, 0.0f, 2.0f, 1.0f));
  auto& sub_shape = canvas.place(std::move(compound));

  std::cout << "Initial:\n" << canvas;
  canvas.move(circle, 10.0f, 10.0f);
  std::cout << "\nAfter move(circle, 10, 10):\n" << canvas;
  canvas.scale(rect, 2.0f);
  std::cout << "\nAfter scale(rect, 2):\n" << canvas;
  canvas.rotate(sub_shape, 3.14159265f / 2.0f);
  std::cout << "\nAfter rotate(sub_shape, 90 deg):\n" << canvas;
  canvas.undo();
  std::cout << "\nAfter undo:\n" << canvas;
  canvas.undo();
  std::cout << "\nAfter undo:\n" << canvas;
  canvas.redo();
  std::cout << "\nAfter redo():\n" << canvas;

  std::cout << "\n";
  benchmark();
  return 0;
}
