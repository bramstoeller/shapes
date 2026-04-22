# shapes

Three takes on a shape hierarchy with undo/redo:

- `classic.cpp`: virtual inheritance, `Operation` struct dispatched by `kind`.
- `closures.cpp`: base class + `std::function` forward/inverse closures.
- `modern.cpp`: `std::variant` + `std::visit`, algebraic inverse. No vtables.

## Benchmark

2000 shapes, 2000 mixed apply + 2000 undo + 2000 redo. Built with `-O3`.

| variant  | apply (ms) | undo (ms) | redo (ms) |
|----------|-----------:|----------:|----------:|
| classic  |       ~60  |      ~21  |      ~30  |
| closures |      ~300  |      ~75  |     ~150  |
| modern   |        ~7  |       ~6  |       ~8  |

Modern wins by roughly 10x over classic and 40x over closures: with `variant` the visit calls inline at `-O3`, classic pays vtable indirection, and closures add `std::function` indirection plus heap-allocated captures.

## Build & run

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/shapes_modern
./build/shapes_classic
./build/shapes_closures
```
