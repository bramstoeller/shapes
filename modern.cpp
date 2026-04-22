#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

struct Shape {
    float x, y;
};

struct Circle : Shape {
    float radius;
};

struct Rectangle : Shape {
    float width, height, orientation = 0.0f;
};

struct Compound;

using ShapeNode = std::variant<Circle, Rectangle, std::unique_ptr<Compound> >;

struct Compound : Shape {
    std::vector<ShapeNode> children;
};

struct BaseOf {
    Shape &operator()(Circle &c) const { return c; }
    Shape &operator()(Rectangle &r) const { return r; }
    Shape &operator()(std::unique_ptr<Compound> &p) const { return *p; }
};

inline Shape &base(ShapeNode &n) { return std::visit(BaseOf{}, n); }

struct Move {
    float dx, dy;

    void operator()(Circle &c) const { c.x += dx; c.y += dy; }
    void operator()(Rectangle &r) const { r.x += dx; r.y += dy; }

    void operator()(Compound &cp) const {
        cp.x += dx; cp.y += dy;
        for (auto &child: cp.children) std::visit(*this, child);
    }

    void operator()(std::unique_ptr<Compound> &p) const { (*this)(*p); }
};

struct Scale {
    float factor;

    void operator()(Circle &c) const { c.radius *= factor; }
    void operator()(Rectangle &r) const { r.width *= factor; r.height *= factor; }

    void operator()(Compound &cp) const {
        for (auto &child: cp.children) {
            const Shape &cb = base(child);
            const float dx = (cb.x - cp.x) * (factor - 1.0f);
            const float dy = (cb.y - cp.y) * (factor - 1.0f);
            std::visit(Move{dx, dy}, child);
            std::visit(*this, child);
        }
    }

    void operator()(std::unique_ptr<Compound> &p) const { (*this)(*p); }
};

struct Rotate {
    float angle;

    void operator()(Circle &) const {}
    void operator()(Rectangle &r) const { r.orientation += angle; }

    void operator()(Compound &cp) const {
        const float co = std::cos(angle);
        const float si = std::sin(angle);
        for (auto &child: cp.children) {
            const Shape &cb = base(child);
            const float rx = cb.x - cp.x;
            const float ry = cb.y - cp.y;
            const float nx = cp.x + rx * co - ry * si;
            const float ny = cp.y + rx * si + ry * co;
            std::visit(Move{nx - cb.x, ny - cb.y}, child);
            std::visit(*this, child);
        }
    }

    void operator()(std::unique_ptr<Compound> &p) const { (*this)(*p); }
};

using Op = std::variant<Move, Scale, Rotate>;

struct Inverter {
    Op operator()(const Move &m) const { return Move{-m.dx, -m.dy}; }
    Op operator()(const Scale &s) const { return Scale{1.0f / s.factor}; }
    Op operator()(const Rotate &r) const { return Rotate{-r.angle}; }
};

inline Op inverse(const Op &op) { return std::visit(Inverter{}, op); }

struct Apply {
    Compound &c;
    void operator()(const Move &m) const { m(c); }
    void operator()(const Scale &s) const { s(c); }
    void operator()(const Rotate &r) const { r(c); }
};

struct Printer {
    std::ostream &os;
    std::string indent;

    void operator()(const Circle &c) const {
        os << indent << "Circle(x=" << c.x << ", y=" << c.y
                << ", radius=" << c.radius << ")\n";
    }

    void operator()(const Rectangle &r) const {
        os << indent << "Rectangle(x=" << r.x << ", y=" << r.y
                << ", width=" << r.width << ", height=" << r.height
                << ", orientation=" << r.orientation << ")\n";
    }

    void operator()(const Compound &cp) const {
        os << indent << "CompoundShape(x=" << cp.x << ", y=" << cp.y << ") {\n";
        const Printer child{os, indent + "  "};
        for (const auto &c: cp.children) std::visit(child, c);
        os << indent << "}\n";
    }

    void operator()(const std::unique_ptr<Compound> &p) const { (*this)(*p); }
};

inline std::ostream &operator<<(std::ostream &os, const ShapeNode &n) {
    std::visit(Printer{os, ""}, n);
    return os;
}

class Canvas : public Compound {
public:
    explicit Canvas(float x = 0.0f, float y = 0.0f) {
        this->x = x;
        this->y = y;
    }

    void place(ShapeNode node) { children.push_back(std::move(node)); }

    void apply(Op op) {
        std::visit(Apply{*this}, op);
        ops_.resize(cursor_);
        ops_.push_back(op);
        ++cursor_;
    }

    bool undo() {
        if (cursor_ == 0) return false;
        --cursor_;
        std::visit(Apply{*this}, inverse(ops_[cursor_]));
        return true;
    }

    bool redo() {
        if (cursor_ >= ops_.size()) return false;
        std::visit(Apply{*this}, ops_[cursor_]);
        ++cursor_;
        return true;
    }

    friend std::ostream &operator<<(std::ostream &os, const Canvas &c) {
        Printer{os, ""}(c);
        return os;
    }

private:
    std::vector<Op> ops_;
    std::size_t cursor_ = 0;
};

void benchmark() {
    constexpr int kShapes = 2000;
    constexpr int kOps = 2000;

    Canvas canvas;
    for (int i = 0; i < kShapes; ++i) {
        if (i % 2 == 0)
            canvas.place(Circle{{float(i), float(i)}, 0.5f});
        else
            canvas.place(Rectangle{{float(i), float(i)}, 2.0f, 1.0f});
    }

    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();
    for (int i = 0; i < kOps; ++i) {
        switch (i % 3) {
            case 0: canvas.apply(Move{0.1f, 0.1f}); break;
            case 1: canvas.apply(Scale{1.001f}); break;
            case 2: canvas.apply(Rotate{0.001f}); break;
        }
    }
    auto t1 = clk::now();
    for (int i = 0; i < kOps; ++i) canvas.undo();
    auto t2 = clk::now();
    for (int i = 0; i < kOps; ++i) canvas.redo();
    auto t3 = clk::now();

    auto us = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };
    std::cout << "[modern]  apply=" << us(t0, t1)
              << "us undo=" << us(t1, t2)
              << "us redo=" << us(t2, t3) << "us\n";
}

int main() {
    Canvas canvas;
    canvas.place(Circle{{1.0f, 0.0f}, 0.5f});
    canvas.place(Rectangle{{-1.0f, 0.0f}, 2.0f, 1.0f});

    auto inner = std::make_unique<Compound>(Compound{{5.0f, 5.0f}, {}});
    inner->children.emplace_back(Circle{{4.0f, 5.0f}, 1.0f});
    inner->children.emplace_back(Rectangle{{6.0f, 5.0f}, 2.0f, 1.0f});
    canvas.place(std::move(inner));

    std::cout << "Initial:\n" << canvas;
    canvas.apply(Move{10.0f, 10.0f});
    std::cout << "\nAfter move(10,10):\n" << canvas;
    canvas.apply(Scale{2.0f});
    std::cout << "\nAfter scale(2):\n" << canvas;
    canvas.apply(Rotate{3.14159265f / 2.0f});
    std::cout << "\nAfter rotate(90 deg):\n" << canvas;

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
