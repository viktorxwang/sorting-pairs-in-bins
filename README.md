# sorting-pairs-in-bins

Scripts and a write-up for **"Sorting Pairs in Bins"**, a puzzle from the "Unsolved Puzzles"
chapter of Peter Winkler's *Mathematical Puzzles: A Connoisseur's Collection* (2004).

> Standing in a row are `n` bins, the `i`-th bin containing two balls numbered `n+1-i`.
> At any time, you may swap two balls from adjacent bins. How many swaps are needed to
> get every ball into the bin carrying its number?

Write `T(n)` for that minimum. The project started as an attempt to find a closed form for
`T(n)`, inspired by Madhavarapu's 2025 resolution of the *Malicious Maître D'* problem from
the same chapter.

## Status

`T(n)` is now known exactly for `n ≤ 8`:

| n | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| `T(n)` | 0 | 2 | 5 | 10 | 15 | 23 | 31 | **40** |
| Winkler's lower bound `⌈n(2n-1)/3⌉` | – | 2 | 5 | 10 | 15 | **22** | 31 | 40 |
| reachable states | 1 | 3 | 21 | 282 | 6,210 | 202,410 | 9,135,630 | 545,007,960 |

`T(8) = 40` was computed here by exhaustive search over all 545,007,960 configurations
(328 s, 520 MiB). It agrees with the conjecture of Ito, Teruyama & Yoshida and **refutes**
this project's earlier conjecture `T(n) = ⌊n²/2⌋ + Σ⌊k/3⌋`, which predicted 41.

The live question is now `n = 6`, the only `n ≤ 20` known to exceed Winkler's lower bound.
No explanation is known.

## Prior work worth reading first

- P. Winkler, *Mathematical Puzzles: A Connoisseur's Collection*, A K Peters, 2004 —
  poses the puzzle and gives the lower bound `⌈C(2n,2)/3⌉`.
- H. Ito, J. Teruyama, Y. Yoshida, ["An almost optimal algorithm for Winkler's sorting pairs
  in bins"](https://www.nii.ac.jp/pi/n9/9_3.pdf), *Progress in Informatics* **9** (2012), 3–7 —
  an algorithm using `< (2/3)n²` swaps, **optimal when `n = 2^m + 1`**, so `T(n)` is known
  exactly for `n = 2, 3, 5, 9, 17, 33, …`. Conjectures `T(n) = ⌈n(2n-1)/3⌉` for all `n ≠ 6`.
- D. West, [REGS problem page](https://dwest.web.illinois.edu/regs/sortpair.html) —
  bounds, a greedy algorithm, and history.

## Programs

Build everything with the 64-bit toolchain:

```powershell
.\build.ps1
```

| file | what it does |
|---|---|
| `sortpairs.cpp` | **the main solver.** Exact `T(n)` for `n ≤ 8` plus an optimal move sequence. |
| `bidirectional_bfs_solver.cpp` | independent hash-table cross-check, `n ≤ 7`. |
| `mine_traces.cpp` | replays an optimal solution and reports its neutral moves and bin type profiles. |
| `center_out_v2.cpp` | the centre-out greedy heuristic, and evidence it never terminates for `n ≥ 5`. |

```bash
./sortpairs.exe lb 12                      # state counts and both lower bounds
./sortpairs.exe selftest 6                 # verify the ranking machinery
./sortpairs.exe exact 7                    # ~6 seconds
./sortpairs.exe exact 8 --save path_n8.txt # ~6 minutes, 520 MiB
./mine_traces.exe --path 8 path_n8.txt     # analyse the n=8 optimal trace
```

### How the n=8 search fits in memory

Three facts make it work:

1. **Perfect hashing.** The reachable configurations are exactly the `n × n` non-negative
   integer matrices with all row and column sums 2, and every one is reachable. Counting
   completions with `F(a,b) = a(1+b)F(a-1,b) + C(a,2)F(a-2,b+2) + C(b,2)F(a,b-2)` ranks them
   bijectively onto `[0, N)`, so the search needs **one byte per state**, not a hash entry.
2. **Reversal symmetry.** Reversing the bin order is a graph automorphism carrying the start
   to the target, so `T(n) = min_x [d(start,x) + d(start,τx)]`. One forward search to depth 20
   settles `T(8)`, instead of 40 — and it needs only one distance array, unlike ordinary
   bidirectional search.
3. **Incremental ranks.** A move preserves the labels held jointly by the two bins it touches,
   so all rank contributions except those two cancel: a neighbour's rank costs O(1).

Result: 199,911,217 states expanded (36.7% of the space) in 328 s.

## Notes

- `.vscode/tasks.json` previously built with 32-bit MinGW 6.3 and **no optimiser**. It now uses
  MSYS2 UCRT64 (x86_64 GCC 15) with `-O3`; `n = 8` is not reachable from a 32-bit build.
- Building `sorting_pairs.tex` needs MiKTeX packages that are not installed here
  (`intcalc`, `booktabs`, and possibly more). Install them with:
  ```powershell
  mpm --install=intcalc --install=booktabs
  ```
