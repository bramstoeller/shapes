#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Shape {
    float x;
    float y;

    struct Operation {
        std::function<void()> forward;
        std::function<void()> inverse;
    };
    std::vector<Operation> ops_;
    std::size_t cursor_ = 0;

    Shape(float x, float y) : x(x), y(y) {
    }

    virtual ~Shape() = default;

    virtual void move(float dx, float dy) {
        record(
            [this, dx, dy] { x += dx; y += dy; },
            [this, dx, dy] { x -= dx; y -= dy; }
        );
    }

    virtual void scale(float factor) = 0;

    virtual void rotate(float rotation) = 0;

    void undo() {
        if (cursor_ == 0) return;
        --cursor_;
        ops_[cursor_].inverse();
    }

    void redo() {
        if (cursor_ >= ops_.size()) return;
        ops_[cursor_].forward();
        ++cursor_;
    }

    virtual void print(std::ostream &os, const std::string &indent, float ox, float oy) const = 0;

protected:
    void record(std::function<void()> forward, std::function<void()> inverse) {
        forward();
        ops_.resize(cursor_);
        ops_.push_back({std::move(forward), std::move(inverse)});
        ++cursor_;
    }
};

inline std::ostream &operator<<(std::ostream &os, const Shape &shape) {
    shape.print(os, "", 0.0f, 0.0f);
    return os;
}

struct Circle : Shape {
    float radius;

    Circle(float x, float y, float radius) : Shape(x, y), radius(radius) {
    }

    void scale(float factor) override {
        record(
            [this, factor] { radius *= factor; },
            [this, factor] { radius /= factor; }
        );
    }

    void rotate(float) override {
        record([] {}, [] {});
    }

    void print(std::ostream &os, const std::string &indent, float ox, float oy) const override {
        os << indent << "Circle(x=" << ox + x << ", y=" << oy + y
           << ", radius=" << radius << ")\n";
    }
};

struct Rectangle : Shape {
    float width;
    float height;
    float orientation;

    Rectangle(float x, float y, float width, float height, float orientation = 0.0f)
        : Shape(x, y), width(width), height(height), orientation(orientation) {
    }

    void scale(float factor) override {
        record(
            [this, factor] { width *= factor; height *= factor; },
            [this, factor] { width /= factor; height /= factor; }
        );
    }

    void rotate(float rotation) override {
        record(
            [this, rotation] { orientation += rotation; },
            [this, rotation] { orientation -= rotation; }
        );
    }

    void print(std::ostream &os, const std::string &indent, float ox, float oy) const override {
        os << indent << "Rectangle(x=" << ox + x << ", y=" << oy + y
           << ", width=" << width << ", height=" << height
           << ", orientation=" << orientation << ")\n";
    }
};

struct CompoundShape : Shape {
    std::vector<std::unique_ptr<Shape> > shapes;

    CompoundShape(float x, float y) : Shape(x, y) {
    }

    void place(std::unique_ptr<Shape> shape) {
        shapes.push_back(std::move(shape));
    }

    void scale(float factor) override {
        record(
            [this, factor] {
                for (auto &s: shapes) {
                    const float dx = s->x * (factor - 1.0f);
                    const float dy = s->y * (factor - 1.0f);
                    s->move(dx, dy);
                    s->scale(factor);
                }
            },
            [this] {
                for (auto &s: shapes) {
                    s->undo();
                    s->undo();
                }
            }
        );
    }

    void rotate(float rotation) override {
        record(
            [this, rotation] {
                const float c = std::cos(rotation);
                const float s = std::sin(rotation);
                for (auto &shape: shapes) {
                    const float rx = shape->x;
                    const float ry = shape->y;
                    const float nx = rx * c - ry * s;
                    const float ny = rx * s + ry * c;
                    shape->move(nx - rx, ny - ry);
                    shape->rotate(rotation);
                }
            },
            [this] {
                for (auto &s: shapes) {
                    s->undo();
                    s->undo();
                }
            }
        );
    }

    void print(std::ostream &os, const std::string &indent, float ox, float oy) const override {
        const float ax = ox + x;
        const float ay = oy + y;
        os << indent << "CompoundShape(x=" << ax << ", y=" << ay << ") {\n";
        for (const auto &shape: shapes) {
            shape->print(os, indent + "  ", ax, ay);
        }
        os << indent << "}\n";
    }
};

void benchmark() {
    constexpr int kShapes = 2000;
    constexpr int kOps = 2000;

    auto canvas = std::make_unique<CompoundShape>(0.0f, 0.0f);
    for (int i = 0; i < kShapes; ++i) {
        if (i % 2 == 0)
            canvas->place(std::make_unique<Circle>(float(i), float(i), 0.5f));
        else
            canvas->place(std::make_unique<Rectangle>(float(i), float(i), 2.0f, 1.0f));
    }

    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();
    for (int i = 0; i < kOps; ++i) {
        switch (i % 3) {
            case 0: canvas->move(0.1f, 0.1f); break;
            case 1: canvas->scale(1.001f); break;
            case 2: canvas->rotate(0.001f); break;
        }
    }
    auto t1 = clk::now();
    for (int i = 0; i < kOps; ++i) canvas->undo();
    auto t2 = clk::now();
    for (int i = 0; i < kOps; ++i) canvas->redo();
    auto t3 = clk::now();

    auto us = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };
    std::cout << "[closures] apply=" << us(t0, t1)
              << "us undo=" << us(t1, t2)
              << "us redo=" << us(t2, t3) << "us\n";
}

int main() {
    auto canvas = std::make_unique<CompoundShape>(0.0f, 0.0f);
    canvas->place(std::make_unique<Circle>(1.0f, 0.0f, 0.5f));
    canvas->place(std::make_unique<Rectangle>(-1.0f, 0.0f, 2.0f, 1.0f));

    auto compound = std::make_unique<CompoundShape>(5.0f, 5.0f);
    compound->place(std::make_unique<Circle>(-1.0f, 0.0f, 1.0f));
    compound->place(std::make_unique<Rectangle>(1.0f, 0.0f, 2.0f, 1.0f));
    canvas->place(std::move(compound));

    std::cout << "Initial:\n" << *canvas;
    canvas->move(10.0f, 10.0f);
    std::cout << "\nAfter move(10,10):\n" << *canvas;
    canvas->scale(2.0f);
    std::cout << "\nAfter scale(2):\n" << *canvas;
    canvas->rotate(3.14159265f / 2.0f);
    std::cout << "\nAfter rotate(90 deg):\n" << *canvas;
    canvas->undo();
    std::cout << "\nAfter undo:\n" << *canvas;
    canvas->undo();
    std::cout << "\nAfter undo:\n" << *canvas;
    canvas->redo();
    std::cout << "\nAfter redo():\n" << *canvas;

    std::cout << "\n";
    benchmark();
    return 0;
}
