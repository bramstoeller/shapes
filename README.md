# Benchmark

1000 shapes, 10000 mixed apply + 10000 undo + 10000 redo. Built with `-O3`.

| implementation | apply (us) | undo (us) | redo (us) |
|----------------|-----------:|----------:|----------:|
| classic        |       ~547 |       ~42 |       ~42 |
| closures       |      ~1022 |       ~42 |       ~42 |
| variant        |       ~178 |       ~18 |       ~18 |

Variant is ~3x faster than classic and ~6x faster than closures on apply (~2x on undo/redo).

