#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Operation {
    enum class Kind { Move, Scale, Rotate } kind;
    float a;
    float b;
};

struct Shape {
    float x;
    float y;
    std::vector<Operation> ops_;
    std::size_t cursor_ = 0;

    Shape(float x, float y) : x(x), y(y) {
    }

    virtual ~Shape() = default;

    void record(Operation op) {
        ops_.resize(cursor_);
        ops_.push_back(op);
        ++cursor_;
    }

    virtual void move(float dx, float dy) {
        record({Operation::Kind::Move, dx, dy});
        x += dx;
        y += dy;
    }

    virtual void scale(float factor) {
        record({Operation::Kind::Scale, factor, 0.0f});
    }

    virtual void rotate(float rotation) {
        record({Operation::Kind::Rotate, rotation, 0.0f});
    }

    virtual void forwardSelf(Operation op) = 0;

    virtual void reverseSelf(Operation op) = 0;

    virtual void undo() {
        if (cursor_ == 0) return;
        --cursor_;
        reverseSelf(ops_[cursor_]);
    }

    virtual void redo() {
        if (cursor_ >= ops_.size()) return;
        forwardSelf(ops_[cursor_]);
        ++cursor_;
    }

    virtual void print(std::ostream &os, const std::string &indent, float ox, float oy) const = 0;
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
        Shape::scale(factor);
        radius *= factor;
    }

    void rotate(float rotation) override {
        Shape::rotate(rotation);
        (void) rotation;
    }

    void forwardSelf(Operation op) override {
        switch (op.kind) {
            case Operation::Kind::Move:   x += op.a; y += op.b; break;
            case Operation::Kind::Scale:  radius *= op.a; break;
            case Operation::Kind::Rotate: break;
        }
    }

    void reverseSelf(Operation op) override {
        switch (op.kind) {
            case Operation::Kind::Move:   x -= op.a; y -= op.b; break;
            case Operation::Kind::Scale:  radius /= op.a; break;
            case Operation::Kind::Rotate: break;
        }
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

    Rectangle(float x, float y, float width, float height, float orientation = 0.0f) : Shape(x, y), width(width),
        height(height), orientation(orientation) {
    }

    void scale(float factor) override {
        Shape::scale(factor);
        width *= factor;
        height *= factor;
    }

    void rotate(float rotation) override {
        Shape::rotate(rotation);
        orientation += rotation;
    }

    void forwardSelf(Operation op) override {
        switch (op.kind) {
            case Operation::Kind::Move:   x += op.a; y += op.b; break;
            case Operation::Kind::Scale:  width *= op.a; height *= op.a; break;
            case Operation::Kind::Rotate: orientation += op.a; break;
        }
    }

    void reverseSelf(Operation op) override {
        switch (op.kind) {
            case Operation::Kind::Move:   x -= op.a; y -= op.b; break;
            case Operation::Kind::Scale:  width /= op.a; height /= op.a; break;
            case Operation::Kind::Rotate: orientation -= op.a; break;
        }
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
        Shape::scale(factor);
        for (auto &shape: shapes) {
            const float dx = shape->x * (factor - 1.0f);
            const float dy = shape->y * (factor - 1.0f);
            shape->move(dx, dy);
            shape->scale(factor);
        }
    }

    void rotate(float rotation) override {
        Shape::rotate(rotation);
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
    }

    void forwardSelf(Operation op) override {
        if (op.kind == Operation::Kind::Move) {
            x += op.a;
            y += op.b;
        }
    }

    void reverseSelf(Operation op) override {
        if (op.kind == Operation::Kind::Move) {
            x -= op.a;
            y -= op.b;
        }
    }

    void undo() override {
        if (cursor_ == 0) return;
        const Operation op = ops_[cursor_ - 1];
        Shape::undo();
        const int n = (op.kind == Operation::Kind::Move) ? 0 : 2;
        for (auto &shape: shapes) {
            for (int i = 0; i < n; ++i) shape->undo();
        }
    }

    void redo() override {
        if (cursor_ >= ops_.size()) return;
        const Operation op = ops_[cursor_];
        Shape::redo();
        const int n = (op.kind == Operation::Kind::Move) ? 0 : 2;
        for (auto &shape: shapes) {
            for (int i = 0; i < n; ++i) shape->redo();
        }
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
    std::cout << "[classic] apply=" << us(t0, t1)
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
