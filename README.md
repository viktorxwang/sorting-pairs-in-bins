sorting-pairs-in-bins
====================

Programs and research notes for "Sorting Pairs in Bins", a puzzle from the "Unsolved Puzzles" chapter of Peter Winkler's Mathematical Puzzles: A Connoisseur's Collection (2004).

## The problem:

There are n bins arranged in a row. Each bin initially contains two balls, both numbered n+1-i for bin i.

For example, with five bins:

Bin 1   Bin 2   Bin 3   Bin 4   Bin 5
  5       4       3       2       1
  5       4       3       2       1

A move consists of swapping two balls from adjacent bins. The goal is to place every ball into the bin carrying its number.

Let T(n) denote the minimum number of swaps required.

This project began as an attempt to find a closed-form expression for T(n), inspired by Madhavarapu's 2025 resolution of another puzzle from the same chapter, Malicious Maître D'.

## Results:

The exact value of T(n) is known for n <= 8.

n       T(n)       Lower bound
1       0          -
2       2          2
3       5          5
4       10         10
5       15         15
6       23         22
7       31         31
8       40         40

Winkler established the lower bound:

T(n) >= ceil(n(2n-1)/3)

For every value in the table except n=6, this bound is achievable.

The value T(8)=40 was established through an exhaustive search of all 545,007,960 possible configurations. The computation took 328 seconds and used approximately 520 MiB of memory, run on my computer. My computer's specs are 32GB RAM and 16GB CPU.

This result disproved an earlier conjecture from this project:

T(n) = floor(n^2/2) + sum(floor(k/3)), for k = 1 to n.

The formula predicted T(8)=41, while the actual value is 40.

The main remaining question is why T(6)=23, one more than Winkler's lower bound. This is the only known exception for n <= 20 to the conjecture that the lower bound is always achievable. No explanation are currently known.

## Previous work:
My work mostly builds off of developing the algorithm. Brute force methods are often not feasible, and so these algorithms must be optimized.

- Peter Winkler, Mathematical Puzzles: A Connoisseur's Collection (2004). Introduces the puzzle and establishes the lower bound.
- H. Ito, J. Teruyama, and Y. Yoshida, "An Almost Optimal Algorithm for Winkler's Sorting Pairs in Bins", Progress in Informatics 9 (2012), 3-7.
  https://www.nii.ac.jp/pi/n9/9_3.pdf

  Develops an algorithm using fewer than (2/3)n^2 swaps. It is optimal when n=2^m+1, giving exact answers for n=2, 3, 5, 9, 17, 33, ... The authors conjecture that Winkler's lower bound is optimal for every n != 6.
- Douglas West, REGS problem page. Collects known bounds, a greedy algorithm, and the history of the problem.
  https://dwest.web.illinois.edu/regs/sortpair.html

## Programs:

sortpairs.cpp
Main solver. Computes exact values for n <= 8 and can save an optimal sequence of moves.

bidirectional_bfs_solver.cpp
Independent hash-table solver used to verify results for n <= 7.

mine_traces.cpp
Analyzes optimal solutions, including neutral moves and bin type profiles.

center_out_v2.cpp
Implements a center-out greedy strategy and provides evidence that it never terminates for n >= 5.

## Building:

Build everything using the 64-bit toolchain:

.\build.ps1

The project uses MSYS2 UCRT64, GCC 15, and the -O3 optimization flag. A 64-bit build is required for the n=8 search.

## Running:

Display state counts and lower bounds:

./sortpairs.exe lb 12

Test the configuration-ranking algorithm:

./sortpairs.exe selftest 6

Find the exact answer for n=7:

./sortpairs.exe exact 7

Find the exact answer for n=8 and save an optimal solution:

./sortpairs.exe exact 8 --save path_n8.txt

Analyze the saved n=8 solution:

./mine_traces.exe --path 8 path_n8.txt

Approximate runtimes:

exact 7: 6 seconds
exact 8: 328 seconds, 520 MiB

## Search method:

The n=8 search uses three ideas to reduce memory usage and computation time.

1. Matrix representation

A configuration can be represented by an n x n matrix. Entry (i,j) records how many balls numbered j are in bin i.

Every row and column sums to 2 because each bin contains two balls and each number appears on exactly two balls.

Every nonnegative integer matrix satisfying these conditions corresponds to a reachable configuration.

This representation allows every configuration to be assigned a unique integer, a technique called perfect hashing. A recursive counting formula calculates these ranks:

F(a,b) = a(1+b)F(a-1,b)
       + C(a,2)F(a-2,b+2)
       + C(b,2)F(a,b-2)

Here, C(a,2) denotes the number of ways to choose two objects from a objects.

The ranks range from 0 to N-1, where N is the total number of configurations. Consequently, the search stores distance information using only one byte per configuration instead of a large hash-table entry.

2. Reversal symmetry

Reversing the order of the bins transforms the starting configuration into the target configuration.

Let tau(x) denote the configuration obtained by reversing the bins in x, and let d(a,b) denote the minimum number of swaps between configurations a and b.

Then:

T(n) = min_x [d(start,x) + d(start,tau(x))]

This allows the search to explore only half the required distance. For n=8, the search depth decreases from 40 to 20, and only one distance array is needed.

3. Incremental ranking

A swap between adjacent bins changes only those two bins. Therefore, most contributions to the configuration's rank remains unchanged.

The solver updates only the affected contributions instead of recalculating the entire rank. This makes generating a neighboring configuration an O(1) operation.

## Performance:

The n=8 search expanded 199,911,217 configurations, approximately 36.7% of the total state space. It completed in 328 seconds using approximately 520 MiB of memory.

Build notes:

- The previous VS Code configuration used 32-bit MinGW 6.3 without optimization. It has been replaced with MSYS2 UCRT64 (64-bit GCC 15) using -O3.
- Building sorting_pairs.tex requires additional MiKTeX packages, including intcalc and booktabs.

Install them through PowerShell:

mpm --install=intcalc --install=booktabs
