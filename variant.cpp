// Less readable than the OOP versions, but included to show that std::variant is ~3x faster than classic
// and ~6x faster than closures on apply (~2x on undo/redo).

#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

constexpr float kPi = 3.14159265358979323846f;

// Shapes
struct Shape {
  // Using public members for simplicity
  float x;
  float y;
};

struct Circle : Shape {
  float radius;
};

struct Rectangle : Shape {
  float width;
  float height;
  float orientation = 0.0f;
};

struct Compound;
using Node = std::variant<Circle, Rectangle, std::unique_ptr<Compound>>;

struct Compound : Shape {
  std::vector<Node> shapes;

  Compound(float x, float y) : Shape{x, y} {}

  Node* place(Node node) {
    shapes.push_back(std::move(node));
    return &shapes.back();
  }
};

// Access x/y on a Node uniformly via the Shape base
inline Shape& as_shape(Circle& c) { return c; }
inline Shape& as_shape(Rectangle& r) { return r; }
inline Shape& as_shape(std::unique_ptr<Compound>& p) { return *p; }
inline Shape& as_shape(Node& n) {
  return std::visit([](auto& s) -> Shape& { return as_shape(s); }, n);
}

// move
inline void move(Shape& s, float dx, float dy) {
  s.x += dx;
  s.y += dy;
}
inline void move(std::unique_ptr<Compound>& p, float dx, float dy) { move(*p, dx, dy); }
inline void move(Node& n, float dx, float dy) {
  std::visit([dx, dy](auto& s) { move(s, dx, dy); }, n);
}

// scale
inline void scale(Circle& c, float factor) { c.radius *= factor; }
inline void scale(Rectangle& r, float factor) {
  r.width *= factor;
  r.height *= factor;
}
inline void scale(Node& n, float factor);
inline void scale(Compound& cp, float factor) {
  // Move and scale child shapes
  for (auto& shape : cp.shapes) {
    Shape& s = as_shape(shape);
    const float dx = s.x * (factor - 1.0f);
    const float dy = s.y * (factor - 1.0f);
    move(shape, dx, dy);
    scale(shape, factor);
  }
}
inline void scale(std::unique_ptr<Compound>& p, float factor) { scale(*p, factor); }
inline void scale(Node& n, float factor) {
  std::visit([factor](auto& s) { scale(s, factor); }, n);
}

// rotate
inline void rotate(Circle&, float) {
  // noop
}
inline void rotate(Rectangle& r, float rotation) {
  // Range is [-pi, pi]
  r.orientation = std::remainder(r.orientation + rotation, 2.0f * kPi);
}
inline void rotate(Node& n, float rotation);
inline void rotate(Compound& cp, float rotation) {
  // Move and rotate child shapes
  const float c = std::cos(rotation);
  const float s = std::sin(rotation);
  for (auto& shape : cp.shapes) {
    Shape& sh = as_shape(shape);
    const float rx = sh.x;
    const float ry = sh.y;
    const float nx = rx * c - ry * s;
    const float ny = rx * s + ry * c;
    move(shape, nx - rx, ny - ry);
    rotate(shape, rotation);
  }
}
inline void rotate(std::unique_ptr<Compound>& p, float rotation) { rotate(*p, rotation); }
inline void rotate(Node& n, float rotation) {
  std::visit([rotation](auto& s) { rotate(s, rotation); }, n);
}

// print
inline void print(std::ostream& os, const Circle& c, const std::string& indent) {
  os << indent << "Circle(x=" << c.x << ", y=" << c.y << ", radius=" << c.radius << ")\n";
}
inline void print(std::ostream& os, const Rectangle& r, const std::string& indent) {
  os << indent << "Rectangle(x=" << r.x << ", y=" << r.y << ", width=" << r.width << ", height=" << r.height
     << ", orientation=" << r.orientation << ")\n";
}
inline void print(std::ostream& os, const Node& n, const std::string& indent);
inline void print(std::ostream& os, const Compound& cp, const std::string& indent) {
  os << indent << "CompoundShape(x=" << cp.x << ", y=" << cp.y << ") {\n";
  for (const auto& shape : cp.shapes) {
    print(os, shape, indent + "  ");
  }
  os << indent << "}\n";
}
inline void print(std::ostream& os, const std::unique_ptr<Compound>& p, const std::string& indent) {
  print(os, *p, indent);
}
inline void print(std::ostream& os, const Node& n, const std::string& indent) {
  std::visit([&](const auto& s) { print(os, s, indent); }, n);
}

inline std::ostream& operator<<(std::ostream& os, const Node& n) {
  print(os, n, "");
  return os;
}

// Transformations
struct Move {
  float dx, dy;
};
struct Scale {
  float factor;
};
struct Rotate {
  float angle_rad;
};
using Operation = std::variant<Move, Scale, Rotate>;

inline void apply(const Move& m, Node& n) { move(n, m.dx, m.dy); }
inline void apply(const Scale& s, Node& n) { scale(n, s.factor); }
inline void apply(const Rotate& r, Node& n) { rotate(n, r.angle_rad); }
inline void apply(const Operation& op, Node& n) {
  std::visit([&n](const auto& o) { ::apply(o, n); }, op);
}

inline void revert(const Move& m, Node& n) { move(n, -m.dx, -m.dy); }
inline void revert(const Scale& s, Node& n) { scale(n, 1.0f / s.factor); }
inline void revert(const Rotate& r, Node& n) { rotate(n, -r.angle_rad); }
inline void revert(const Operation& op, Node& n) {
  std::visit([&n](const auto& o) { revert(o, n); }, op);
}

// Canvas
class Canvas {

  using History = std::vector<std::pair<Node*, Operation>>;

  // Deque keeps Node references stable
  std::deque<Node> shapes_;

  History history_;
  size_t cursor_ = 0;

public:
  Node& place(Node node) {
    shapes_.push_back(std::move(node));
    return shapes_.back();
  }

  void move(Node& shape, float dx, float dy) {
    ::move(shape, dx, dy);
    remember(shape, Move{dx, dy});
  }

  void scale(Node& shape, float factor) {
    ::scale(shape, factor);
    remember(shape, Scale{factor});
  }

  void rotate(Node& shape, float angle_rad) {
    ::rotate(shape, angle_rad);
    remember(shape, Rotate{angle_rad});
  }

  void undo() {
    if (cursor_ == 0) return;
    --cursor_;
    auto& [shape, op] = history_[cursor_];
    ::revert(op, *shape);
  }

  void redo() {
    if (cursor_ == history_.size()) return;
    auto& [shape, op] = history_[cursor_];
    ::apply(op, *shape);
    ++cursor_;
  }

  friend std::ostream& operator<<(std::ostream& os, const Canvas& c) {
    for (const auto& shape : c.shapes_) {
      os << shape;
    }
    return os;
  }

private:
  void remember(Node& shape, Operation op) {
    history_.resize(cursor_);
    history_.emplace_back(&shape, std::move(op));
    cursor_ = history_.size();
  }
};

void benchmark() {
  constexpr int kShapes = 1000;
  constexpr int kOps = 10000;

  Canvas canvas;
  std::vector<Node*> shapes;
  shapes.reserve(kShapes);
  for (int i = 0; i < kShapes; ++i) {
    if (i % 2 == 0) {
      shapes.push_back(&canvas.place(Circle{float(i), float(i), 0.5f}));
    } else {
      shapes.push_back(&canvas.place(Rectangle{float(i), float(i), 2.0f, 1.0f}));
    }
  }

  using clk = std::chrono::steady_clock;
  auto t0 = clk::now();
  for (int i = 0; i < kOps; ++i) {
    Node& s = *shapes[i % kShapes];
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
  std::cout << "[variant] apply=" << us(t0, t1) << "us undo=" << us(t1, t2) << "us redo=" << us(t2, t3) << "us\n";
}

int main() {
  Canvas canvas;
  auto& circle = canvas.place(Circle{1.0f, 0.0f, 0.5f});
  auto& rect = canvas.place(Rectangle{-1.0f, 0.0f, 2.0f, 1.0f});

  auto compound = std::make_unique<Compound>(5.0f, 5.0f);
  compound->place(Circle{-1.0f, 0.0f, 1.0f});
  compound->place(Rectangle{1.0f, 0.0f, 2.0f, 1.0f});
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
