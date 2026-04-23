# shapes

Three takes on a shape hierarchy with undo/redo:

- `classic.cpp`: virtual inheritance, `Operation` struct dispatched by `kind`.
- `closures.cpp`: base class + `std::function` forward/inverse closures.
- `modern.cpp`: `std::variant` + `std::visit`, algebraic inverse. No vtables.

## Benchmark

2000 shapes, 2000 mixed apply + 2000 undo + 2000 redo. Built with `-O3`.

| variant  | apply (ms) | undo (ms) | redo (ms) |
|----------|-----------:|----------:|----------:|
| classic  |       ~95  |      ~53  |      ~54  |
| closures |      ~400  |      ~89  |     ~212  |
| modern   |        ~9  |       ~9  |      ~10  |

Modern wins by roughly 10x over classic and 45x over closures: with `variant` the visit calls inline at `-O3`, classic pays vtable indirection on every `do_move`/`do_scale`/`do_rotate` hook, and closures add `std::function` indirection plus heap-allocated captures.

## Build & run

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/shapes_modern
./build/shapes_classic
./build/shapes_closures
```
