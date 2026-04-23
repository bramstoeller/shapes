# Benchmark

2000 shapes, 2000 mixed apply + 2000 undo + 2000 redo. Built with `-O3`. 10-run medians.

| implementation | apply (ms) | undo (ms) | redo (ms) |
|----------------|-----------:|----------:|----------:|
| classic        |        ~51 |       ~19 |       ~25 |
| closures       |       ~209 |       ~61 |      ~102 |
| variant        |       ~2.5 |      ~2.4 |      ~2.8 |

Variant is ~20x faster than classic and ~80x faster than closures on apply (8–10x and 25–35x on undo/redo).

