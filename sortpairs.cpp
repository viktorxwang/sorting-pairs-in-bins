// ---------------------------------------------------------------------------
// sortpairs.cpp -- exact solver for Winkler's "Sorting Pairs in Bins".
//
//   n bins in a row; bin i starts with two balls labelled n+1-i.  A move swaps
//   one ball of bin i with one ball of bin i+1.  T(n) is the least number of
//   moves reaching the configuration where bin i holds two balls labelled i.
//
// The point of this program is that the reachable state space is *exactly* the
// set of n x n non-negative integer matrices with all row and column sums 2
// (rows = labels, columns = bins), counted by
//
//     1, 3, 21, 282, 6210, 202410, 9135630, 545007960, ...
//
// so states can be ranked into a contiguous range [0, N) and the whole search
// becomes a flat byte array instead of a hash table.  For n = 8 that is
// 545,007,960 states = 520 MiB, which fits in RAM; the previous hash-table
// solver did not get close.
//
// Two further reductions:
//
//   * Reversing the bin order is a graph automorphism tau with tau(start) =
//     target.  Hence dist(x, target) = dist(tau(x), start), so a *single*
//     forward BFS suffices and
//
//         T(n) = min over x of [ dist(start,x) + dist(start,tau(x)) ].
//
//     BFS to depth D certifies every T <= 2D, so n = 8 needs depth 20, not 40.
//
//   * Ranks of neighbours are computed incrementally.  A move at boundary i
//     leaves the multiset of labels in bins i+2..n-1 untouched, so all rank
//     contributions except those of bins i and i+1 cancel.
//
// Subcommands:
//   sortpairs lb <n>          lower bounds (displacement and Winkler)
//   sortpairs exact <n>       exact T(n) + an optimal move sequence
//   sortpairs selftest <n>    check ranking/unranking and incremental ranks
//
// Build (64-bit compiler required for n = 8):
//   g++ -O3 -march=native -DNDEBUG -o sortpairs.exe sortpairs.cpp
// ---------------------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace std;
typedef uint64_t u64;

static const int MAXN = 16;

// ---------------------------------------------------------------------------
// F[a][b] = number of ways to fill the remaining (2a+b)/2 bins when a labels
// still have two copies unplaced and b labels have one copy unplaced.  This is
// the completion count that makes the ranking work, and F[n][0] is the size of
// the whole state space.
// ---------------------------------------------------------------------------
static u64 F[MAXN + 1][2 * MAXN + 3];

static void build_F(int n) {
    memset(F, 0, sizeof(F));
    F[0][0] = 1;

    // Every term of the recurrence removes two balls, so fill by total count.
    for (int m = 2; m <= 2 * n; m += 2) {
        for (int a = 0; a <= n; a++) {
            int b = m - 2 * a;
            if (b < 0 || b > n || a + b > n) continue;

            u64 v = 0;

            // (x,x) with x still doubled, or (x,y) with one doubled and one
            // single: both leave a-1 doubled labels and b singles.
            if (a >= 1) v += (u64)a * (u64)(1 + b) * F[a - 1][b];

            // (x,y), both doubled: two labels drop from doubled to single.
            if (a >= 2) v += (u64)(a * (a - 1) / 2) * F[a - 2][b + 2];

            // (x,y), both single: two labels are finished.
            if (b >= 2) v += (u64)(b * (b - 1) / 2) * F[a][b - 2];

            F[a][b] = v;
        }
    }
}

// ---------------------------------------------------------------------------
// State: n bins, bin i holding the sorted pair (lo[i], hi[i]) of labels 1..n.
// ---------------------------------------------------------------------------
static int n_;   // number of bins
static u64 N_;   // number of states

struct State {
    int lo[MAXN], hi[MAXN];

    // Only the first n_ bins are meaningful; the rest of the array is padding.
    bool operator==(const State& o) const {
        for (int i = 0; i < n_; i++)
            if (lo[i] != o.lo[i] || hi[i] != o.hi[i]) return false;
        return true;
    }
};

static State start_state, target_state;

static void make_endpoints() {
    for (int i = 0; i < n_; i++) {
        start_state.lo[i] = start_state.hi[i] = n_ - i;
        target_state.lo[i] = target_state.hi[i] = i + 1;
    }
}

// Reverse the bin order.  tau(start) = target, tau is an involution, and a move
// at boundary i becomes a move at boundary n-2-i.
static inline State tau(const State& s) {
    State r;
    for (int i = 0; i < n_; i++) {
        r.lo[i] = s.lo[n_ - 1 - i];
        r.hi[i] = s.hi[n_ - 1 - i];
    }
    return r;
}

// ---------------------------------------------------------------------------
// Ranking.
//
// Bins are filled left to right.  At bin i the remaining labels form a multiset
// described by counts c[1..n] in {0,1,2}, summarised by a (labels with two
// copies left) and b (labels with one).  Pairs are ordered lexicographically,
// and the number of completions after choosing a pair depends only on which of
// four classes the pair falls into -- so a contribution is three multiplies.
// ---------------------------------------------------------------------------
static inline u64 contrib(const int* c, int a, int b, int x0, int y0) {
    u64 cnt2 = 0;  // both labels still doubled          -> F[a-2][b+2]
    u64 cntm = 0;  // (x,x), or one doubled + one single -> F[a-1][b]
    u64 cnt1 = 0;  // both labels single                 -> F[a][b-2]

    for (int x = 1; x <= x0; x++) {
        if (!c[x]) continue;

        int ymax = (x < x0) ? n_ : y0 - 1;
        if (x > ymax) continue;

        if (c[x] == 2) {
            cntm++;  // the pair (x,x)
            for (int y = x + 1; y <= ymax; y++) {
                if (c[y] == 2) cnt2++;
                else if (c[y] == 1) cntm++;
            }
        } else {
            for (int y = x + 1; y <= ymax; y++) {
                if (c[y] == 2) cntm++;
                else if (c[y] == 1) cnt1++;
            }
        }
    }

    u64 r = 0;
    if (cnt2) r += cnt2 * F[a - 2][b + 2];
    if (cntm) r += cntm * F[a - 1][b];
    if (cnt1) r += cnt1 * F[a][b - 2];
    return r;
}

static inline void take(int* c, int& a, int& b, int v) {
    if (c[v] == 2) { c[v] = 1; a--; b++; }
    else           { c[v] = 0; b--;      }
}

static u64 rank_state(const State& s) {
    int c[MAXN + 1];
    for (int i = 1; i <= n_; i++) c[i] = 2;

    int a = n_, b = 0;
    u64 r = 0;

    for (int i = 0; i < n_; i++) {
        r += contrib(c, a, b, s.lo[i], s.hi[i]);
        take(c, a, b, s.lo[i]);
        take(c, a, b, s.hi[i]);
    }
    return r;
}

static void unrank_state(u64 r, State& s) {
    int c[MAXN + 1];
    for (int i = 1; i <= n_; i++) c[i] = 2;

    int a = n_, b = 0;

    for (int i = 0; i < n_; i++) {
        bool placed = false;

        for (int x = 1; x <= n_ && !placed; x++) {
            if (!c[x]) continue;

            for (int y = x; y <= n_; y++) {
                if (y == x) { if (c[x] < 2) continue; }
                else if (!c[y]) continue;

                u64 cnt;
                if (x == y)                          cnt = F[a - 1][b];
                else if (c[x] == 2 && c[y] == 2)     cnt = F[a - 2][b + 2];
                else if (c[x] == 1 && c[y] == 1)     cnt = F[a][b - 2];
                else                                 cnt = F[a - 1][b];

                if (r < cnt) {
                    s.lo[i] = x;
                    s.hi[i] = y;
                    placed = true;
                    break;
                }
                r -= cnt;
            }
        }

        take(c, a, b, s.lo[i]);
        take(c, a, b, s.hi[i]);
    }
}

// ---------------------------------------------------------------------------
// Neighbour expansion.
//
// visit(newRank, boundary, newLoA, newHiA, newLoB, newHiB, give, recv) is called
// once per distinct successor, where `give` is the label handed from bin i to
// bin i+1 and `recv` the label handed the other way.  Ranks are derived from the
// parent's rank: bins before i and after i+1 contribute identically, because a
// move preserves the multiset of labels held by bins i and i+1 jointly.
// ---------------------------------------------------------------------------
struct Expander {
    // Per-bin snapshots of the remaining-label multiset, and each bin's own
    // rank contribution, recorded while ranking the parent.
    int  csnap[MAXN][MAXN + 1];
    int  asnap[MAXN], bsnap[MAXN];
    u64  pc[MAXN];

    void prepare(const State& s) {
        int c[MAXN + 1];
        for (int i = 1; i <= n_; i++) c[i] = 2;

        int a = n_, b = 0;

        for (int i = 0; i < n_; i++) {
            memcpy(csnap[i], c, sizeof(int) * (n_ + 1));
            asnap[i] = a;
            bsnap[i] = b;

            pc[i] = contrib(c, a, b, s.lo[i], s.hi[i]);

            take(c, a, b, s.lo[i]);
            take(c, a, b, s.hi[i]);
        }
    }

    template <typename F_>
    void expand(const State& s, u64 r, F_ visit) {
        prepare(s);

        for (int i = 0; i + 1 < n_; i++) {
            int p = s.lo[i],     q = s.hi[i];
            int u = s.lo[i + 1], v = s.hi[i + 1];

            // Choosing the ball to give away from each bin; when a bin holds a
            // matched pair the two choices coincide.
            int leftPick[2]  = {p, q};
            int rightPick[2] = {u, v};
            int nl = (p == q) ? 1 : 2;
            int nr = (u == v) ? 1 : 2;

            u64 base = r - pc[i] - pc[i + 1];

            for (int li = 0; li < nl; li++) {
                for (int ri = 0; ri < nr; ri++) {
                    int give  = leftPick[li];   // leaves bin i
                    int recv  = rightPick[ri];  // leaves bin i+1

                    if (give == recv) continue;  // identity move

                    int keepL = (li == 0) ? q : p;
                    int keepR = (ri == 0) ? v : u;

                    int aLo = min(keepL, recv),  aHi = max(keepL, recv);
                    int bLo = min(keepR, give),  bHi = max(keepR, give);

                    u64 nrank = base;
                    nrank += contrib(csnap[i], asnap[i], bsnap[i], aLo, aHi);

                    // Multiset seen by bin i+1 after the new bin i is fixed.
                    int c2[MAXN + 1];
                    memcpy(c2, csnap[i], sizeof(int) * (n_ + 1));
                    int a2 = asnap[i], b2 = bsnap[i];
                    take(c2, a2, b2, aLo);
                    take(c2, a2, b2, aHi);

                    nrank += contrib(c2, a2, b2, bLo, bHi);

                    visit(nrank, i, aLo, aHi, bLo, bHi, give, recv);
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Lower bounds.
// ---------------------------------------------------------------------------

// Total displacement / 2: every move changes total displacement by at most 2.
static u64 lb_displacement(int n) { return (u64)n * n / 2; }

// Winkler's lower bound, ceil(C(2n,2)/3), as stated in Winkler's book and on
// D. West's REGS page, and used as the benchmark by Ito, Teruyama & Yoshida
// (2012).  The counting argument charges one unit to each of the C(2n,2) pairs
// of balls and observes that a single swap can discharge at most three.  It is
// attained for every n <= 20 except n = 6, and proved attained for n = 2^m + 1.
// n = 1 is degenerate (nothing to sort), so it is reported as 0.
static u64 lb_winkler(int n) {
    if (n <= 1) return 0;
    u64 pairs = (u64)n * (2ULL * n - 1);  // C(2n,2)
    return (pairs + 2) / 3;               // ceil(pairs/3)
}

// ---------------------------------------------------------------------------
// Move sequence handling.
// ---------------------------------------------------------------------------
struct Move {
    int boundary;  // 0-based boundary between bins boundary and boundary+1
    int give;      // label sent right
    int recv;      // label sent left
};

static bool apply_move(State& s, const Move& m) {
    int i = m.boundary;

    int p = s.lo[i], q = s.hi[i];
    int u = s.lo[i + 1], v = s.hi[i + 1];

    int keepL;
    if      (p == m.give) keepL = q;
    else if (q == m.give) keepL = p;
    else return false;

    int keepR;
    if      (u == m.recv) keepR = v;
    else if (v == m.recv) keepR = u;
    else return false;

    s.lo[i]     = min(keepL, m.recv);  s.hi[i]     = max(keepL, m.recv);
    s.lo[i + 1] = min(keepR, m.give);  s.hi[i + 1] = max(keepR, m.give);
    return true;
}

// Total displacement, used to classify a move as useful / neutral / regressive.
static int displacement(const State& s) {
    int d = 0;
    for (int i = 0; i < n_; i++) {
        d += abs(i - (s.lo[i] - 1));
        d += abs(i - (s.hi[i] - 1));
    }
    return d;
}

// ---------------------------------------------------------------------------
// Exact search.
// ---------------------------------------------------------------------------

static vector<uint8_t> dist_;

// One step back down the BFS tree: find any predecessor `out` at distance d-1,
// and record the move that carries `out` forward to `s`.
//
// Moves are involutions, so expanding `s` and finding a successor at distance
// d-1 also identifies a predecessor.  The recorded move must be the reverse of
// the one the expansion reports: if s -> out sends `give` right and `recv` left,
// then out -> s sends `recv` right and `give` left.
static bool step_back(const State& s, u64 r, int d, State& out, u64& outR, Move& mv) {
    Expander ex;
    bool found = false;

    ex.expand(s, r, [&](u64 nr, int i, int aLo, int aHi, int bLo, int bHi,
                        int give, int recv) {
        if (found) return;
        if (dist_[nr] != (uint8_t)(d - 1)) return;

        found = true;
        outR = nr;

        out = s;
        out.lo[i] = aLo;      out.hi[i] = aHi;
        out.lo[i + 1] = bLo;  out.hi[i + 1] = bHi;

        mv.boundary = i;
        mv.give = recv;
        mv.recv = give;
    });

    return found;
}

static void print_path(const vector<Move>& path) {
    State s = start_state;

    printf("\noptimal move sequence (%zu moves)\n", path.size());
    printf("  move  boundary  send-right  send-left  kind      state\n");

    for (size_t k = 0; k < path.size(); k++) {
        int before = displacement(s);
        if (!apply_move(s, path[k])) {
            printf("  !! move %zu is not applicable -- path is corrupt\n", k + 1);
            return;
        }
        int after = displacement(s);

        const char* kind = (after < before) ? "useful"
                         : (after == before) ? "neutral" : "regressive";

        printf("  %4zu  %8d  %10d  %9d  %-10s ",
               k + 1, path[k].boundary, path[k].give, path[k].recv, kind);

        for (int i = 0; i < n_; i++) printf("(%d%d)", s.lo[i], s.hi[i]);
        printf("\n");
    }

    printf("\nreached target: %s\n", (s == target_state) ? "yes" : "NO");
}

static int solve_exact(int n, bool wantPath, const char* saveFile) {
    n_ = n;
    build_F(n);
    N_ = F[n][0];
    make_endpoints();

    printf("n = %d\n", n);
    printf("state space          : %llu\n", (unsigned long long)N_);
    printf("displacement bound   : %llu\n", (unsigned long long)lb_displacement(n));
    printf("Winkler bound        : %llu\n", (unsigned long long)lb_winkler(n));
    fflush(stdout);

    u64 sr = rank_state(start_state);
    u64 tr = rank_state(target_state);

    if (sr == tr) {
        printf("\nT(%d) = 0\n", n);
        return 0;
    }

    dist_.assign(N_, 0xFF);
    dist_[sr] = 0;

    int best = INT32_MAX;
    u64 bestX = 0;
    int bestDx = 0;

    auto t0 = chrono::steady_clock::now();
    Expander ex;
    State s;

    for (int d = 0; d < 200; d++) {
        u64 levelCount = 0, discovered = 0;

        for (u64 r = 0; r < N_; r++) {
            if (dist_[r] != (uint8_t)d) continue;
            levelCount++;

            unrank_state(r, s);

            // Meet in the middle through the bin-reversal automorphism.
            u64 r2 = rank_state(tau(s));
            if (dist_[r2] != 0xFF) {
                int tot = d + dist_[r2];
                if (tot < best) { best = tot; bestX = r; bestDx = d; }
            }

            ex.expand(s, r, [&](u64 nr, int, int, int, int, int, int, int) {
                if (dist_[nr] == 0xFF) {
                    dist_[nr] = (uint8_t)(d + 1);
                    discovered++;
                }
            });
        }

        double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        printf("  depth %2d : %12llu states, %12llu new, best=%s, %.1fs\n",
               d, (unsigned long long)levelCount, (unsigned long long)discovered,
               (best == INT32_MAX ? "-" : to_string(best).c_str()), el);
        fflush(stdout);

        // Everything of length <= 2d has been seen by now.
        if (best <= 2 * d) break;

        if (discovered == 0 && levelCount == 0) {
            printf("  search exhausted without meeting\n");
            break;
        }
    }

    if (best == INT32_MAX) {
        printf("\nno solution found\n");
        return -1;
    }

    printf("\nT(%d) = %d\n", n, best);
    printf("  displacement bound %llu, Winkler bound %llu -> %s\n",
           (unsigned long long)lb_displacement(n),
           (unsigned long long)lb_winkler(n),
           (best == (int)lb_winkler(n) ? "MEETS Winkler's lower bound (optimal)"
                                       : "exceeds Winkler's lower bound"));

    if (!wantPath) return best;

    // Walk back from the meeting state to the start on both sides.
    State x;
    unrank_state(bestX, x);

    vector<Move> firstHalf;
    {
        State cur = x;
        u64 curR = bestX;
        for (int d = bestDx; d > 0; d--) {
            State prev; u64 prevR; Move mv;
            if (!step_back(cur, curR, d, prev, prevR, mv)) {
                printf("  path reconstruction failed at depth %d\n", d);
                return best;
            }
            firstHalf.push_back(mv);
            cur = prev; curR = prevR;
        }
        reverse(firstHalf.begin(), firstHalf.end());
    }

    // dist(x, target) = dist(tau(x), start); walking tau(x) back to start and
    // mapping through tau gives target -> x, which reversed is x -> target.
    vector<Move> secondHalf;
    {
        State tx = tau(x);
        u64 txR = rank_state(tx);
        int d2 = dist_[txR];

        State cur = tx;
        u64 curR = txR;
        vector<Move> back;

        for (int d = d2; d > 0; d--) {
            State prev; u64 prevR; Move mv;
            if (!step_back(cur, curR, d, prev, prevR, mv)) {
                printf("  path reconstruction failed on second half at depth %d\n", d);
                return best;
            }
            back.push_back(mv);
            cur = prev; curR = prevR;
        }

        // `back` lists the start -> tau(x) path in reverse order.  Mapping a
        // move through tau flips the boundary about the centre and exchanges
        // give/recv; reversing the path direction exchanges them back.  So
        // reading `back` in its natural order and only flipping the boundary
        // yields the x -> target half directly.
        for (size_t k = 0; k < back.size(); k++) {
            Move m = back[k];
            secondHalf.push_back(Move{n_ - 2 - m.boundary, m.give, m.recv});
        }
    }

    vector<Move> path = firstHalf;
    path.insert(path.end(), secondHalf.begin(), secondHalf.end());
    print_path(path);

    if (saveFile) {
        FILE* f = fopen(saveFile, "w");
        if (!f) {
            printf("  could not write %s\n", saveFile);
        } else {
            for (const Move& m : path)
                fprintf(f, "%d %d %d\n", m.boundary, m.give, m.recv);
            fclose(f);
            printf("  move sequence written to %s\n", saveFile);
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// Self test: ranking must be a bijection onto [0, N), and incremental neighbour
// ranks must agree with ranking from scratch.
// ---------------------------------------------------------------------------
static int selftest(int n) {
    n_ = n;
    build_F(n);
    N_ = F[n][0];
    make_endpoints();

    printf("n = %d, state space = %llu\n", n, (unsigned long long)N_);

    vector<uint8_t> seen(N_, 0);
    State s;
    Expander ex;
    u64 bad = 0;

    for (u64 r = 0; r < N_; r++) {
        unrank_state(r, s);

        u64 back = rank_state(s);
        if (back != r) {
            if (bad++ < 5) printf("  rank/unrank mismatch: %llu -> %llu\n",
                                  (unsigned long long)r, (unsigned long long)back);
            continue;
        }
        seen[r] = 1;

        // Verify every label appears exactly twice.
        int cnt[MAXN + 1] = {0};
        for (int i = 0; i < n_; i++) { cnt[s.lo[i]]++; cnt[s.hi[i]]++; }
        for (int l = 1; l <= n_; l++)
            if (cnt[l] != 2 && bad++ < 5)
                printf("  bad multiset at rank %llu\n", (unsigned long long)r);

        ex.expand(s, r, [&](u64 nr, int i, int aLo, int aHi, int bLo, int bHi,
                            int, int) {
            State ns = s;
            ns.lo[i] = aLo;      ns.hi[i] = aHi;
            ns.lo[i + 1] = bLo;  ns.hi[i + 1] = bHi;

            u64 truth = rank_state(ns);
            if (truth != nr && bad++ < 5)
                printf("  incremental rank mismatch at %llu: got %llu want %llu\n",
                       (unsigned long long)r, (unsigned long long)nr,
                       (unsigned long long)truth);
        });
    }

    u64 missing = 0;
    for (u64 r = 0; r < N_; r++) if (!seen[r]) missing++;

    printf("  ranking is a bijection : %s\n", missing ? "NO" : "yes");
    printf("  incremental ranks ok   : %s\n", bad ? "NO" : "yes");
    printf("  tau(start) == target   : %s\n",
           (tau(start_state) == target_state) ? "yes" : "NO");

    return (bad || missing) ? 1 : 0;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage:\n"
                "  %s lb <maxn>        lower bounds for 1..maxn\n"
                "  %s exact <n> [--save f]  exact T(n), optimal move sequence\n"
                "  %s selftest <n>     verify ranking machinery\n",
                argv[0], argv[0], argv[0]);
        return 2;
    }

    string cmd = argv[1];

    if (cmd == "lb") {
        int mx = (argc > 2) ? atoi(argv[2]) : 12;
        printf("%4s %14s %14s %14s\n", "n", "states", "displacement", "Winkler");
        for (int n = 1; n <= mx; n++) {
            build_F(n);
            printf("%4d %14llu %14llu %14llu\n", n,
                   (unsigned long long)F[n][0],
                   (unsigned long long)lb_displacement(n),
                   (unsigned long long)lb_winkler(n));
        }
        return 0;
    }

    if (argc < 3) { fprintf(stderr, "missing n\n"); return 2; }
    int n = atoi(argv[2]);
    if (n < 1 || n > MAXN) { fprintf(stderr, "n out of range\n"); return 2; }

    if (cmd == "selftest") return selftest(n);

    if (cmd == "exact") {
        const char* save = nullptr;
        for (int k = 3; k + 1 < argc; k++)
            if (!strcmp(argv[k], "--save")) save = argv[k + 1];
        return solve_exact(n, true, save) < 0 ? 1 : 0;
    }

    fprintf(stderr, "unknown command '%s'\n", cmd.c_str());
    return 2;
}
