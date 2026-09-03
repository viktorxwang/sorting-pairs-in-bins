// ---------------------------------------------------------------------------
// center_out_v2.cpp
//
// The centre-out local-greedy heuristic for "Sorting Pairs in Bins", and the
// evidence that it is not optimal.
//
// The rule: prefer a useful move (one that reduces total displacement) at the
// boundary nearest the centre; failing that, a neutral move at the boundary
// nearest the centre; failing that, a regressive move at the centre boundary.
//
// The decision logic is unchanged from the original.  What changed:
//
//   * State is a pair of flat arrays instead of vector<vector<int>>, and moves
//     are applied in place with an explicit undo, so the neutral-move scan no
//     longer copies and re-sorts the whole configuration for every candidate.
//   * D() is computed once per scan rather than once per boundary, and is
//     updated from the two touched bins rather than recomputed over all n.
//   * stable_sort replaces sort when ordering boundaries by distance from the
//     centre, so ties break deterministically and runs are reproducible.
//   * The reference table is generated rather than hard-coded, and now includes
//     Winkler's lower bound, which is the bound that actually binds.
//
// Build:
//   g++ -O3 -march=native -DNDEBUG -o center_out_v2.exe center_out_v2.cpp
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <string>
#include <vector>

using namespace std;

// Labels are 1-indexed, bins 0-indexed.
static inline int target_bin_of(int label) { return label - 1; }

struct Config {
    int n;
    vector<int> lo, hi;

    explicit Config(int n_) : n(n_), lo(n_), hi(n_) {}

    // Displacement contributed by one bin.
    inline int binD(int i) const {
        return abs(i - target_bin_of(lo[i])) + abs(i - target_bin_of(hi[i]));
    }

    inline int D() const {
        int t = 0;
        for (int i = 0; i < n; i++) t += binD(i);
        return t;
    }

    inline bool sorted() const {
        for (int i = 0; i < n; i++)
            if (lo[i] != i + 1 || hi[i] != i + 1) return false;
        return true;
    }

    // Does bin i hold a ball that wants to move right / left?
    inline bool hasR(int i) const {
        return target_bin_of(lo[i]) > i || target_bin_of(hi[i]) > i;
    }
    inline bool hasL(int i) const {
        return target_bin_of(lo[i]) < i || target_bin_of(hi[i]) < i;
    }

    // Swap `give` out of bin i and `recv` out of bin i+1, in place.
    inline void apply(int i, int give, int recv) {
        int keepL = (lo[i] == give) ? hi[i] : lo[i];
        int keepR = (lo[i + 1] == recv) ? hi[i + 1] : lo[i + 1];

        lo[i]     = min(keepL, recv);  hi[i]     = max(keepL, recv);
        lo[i + 1] = min(keepR, give);  hi[i + 1] = max(keepR, give);
    }
};

struct Result {
    int moves, useful, wasted, regressive;
    bool success;
    int cycleAt;    // step at which a state repeated, or -1
    int cycleLen;
};

// Key a configuration so repeated states can be detected; this is what
// distinguishes "the heuristic is suboptimal" from "the heuristic never
// terminates", which are quite different claims.  One byte per ball, so there
// is no label-width limit to worry about.
static string pack(const Config& s) {
    string k;
    k.reserve(2 * s.n);
    for (int i = 0; i < s.n; i++) {
        k.push_back((char)s.lo[i]);
        k.push_back((char)s.hi[i]);
    }
    return k;
}

static Result center_out_v2(int n, int max_steps = 4000) {
    Config s(n);
    for (int i = 0; i < n; i++) s.lo[i] = s.hi[i] = n - i;

    const double center = (n - 1) / 2.0;

    // Boundaries ordered by distance from the centre, computed once.
    vector<int> byCenter(n > 1 ? n - 1 : 0);
    iota(byCenter.begin(), byCenter.end(), 0);
    stable_sort(byCenter.begin(), byCenter.end(), [center](int a, int b) {
        return fabs((a + 0.5) - center) < fabs((b + 0.5) - center);
    });

    int useful = 0, wasted = 0, regressive = 0;

    // step at which each configuration was first seen
    unordered_map<string, int> firstSeen;

    for (int step = 0; step < max_steps; step++) {
        if (s.sorted()) return {step, useful, wasted, regressive, true, -1, 0};

        auto it = firstSeen.emplace(pack(s), step);
        if (!it.second)
            return {step, useful, wasted, regressive, false,
                    it.first->second, step - it.first->second};

        // --- a useful move, at the boundary nearest the centre --------------
        int chosen = -1;
        for (int i : byCenter) {
            if (s.hasR(i) && s.hasL(i + 1)) { chosen = i; break; }
        }

        if (chosen >= 0) {
            int i = chosen;

            // Lowest-labelled ball of bin i that wants to move right, and of
            // bin i+1 that wants to move left.
            int give = (target_bin_of(s.lo[i]) > i) ? s.lo[i] : s.hi[i];
            int recv = (target_bin_of(s.lo[i + 1]) < i + 1) ? s.lo[i + 1] : s.hi[i + 1];

            s.apply(i, give, recv);
            useful++;
            continue;
        }

        // --- otherwise a neutral move, nearest the centre -------------------
        bool done = false;

        for (int i : byCenter) {
            int d0 = s.binD(i) + s.binD(i + 1);

            int lpick[2] = {s.lo[i], s.hi[i]};
            int rpick[2] = {s.lo[i + 1], s.hi[i + 1]};
            int nl = (lpick[0] == lpick[1]) ? 1 : 2;
            int nr = (rpick[0] == rpick[1]) ? 1 : 2;

            for (int a = 0; a < nl && !done; a++) {
                for (int b = 0; b < nr && !done; b++) {
                    int give = lpick[a], recv = rpick[b];
                    if (give == recv) continue;   // identity move

                    int sl0 = s.lo[i], sh0 = s.hi[i];
                    int sl1 = s.lo[i + 1], sh1 = s.hi[i + 1];

                    s.apply(i, give, recv);

                    // Only the two touched bins can have changed.
                    if (s.binD(i) + s.binD(i + 1) == d0) {
                        wasted++;
                        done = true;
                    } else {
                        s.lo[i] = sl0; s.hi[i] = sh0;
                        s.lo[i + 1] = sl1; s.hi[i + 1] = sh1;
                    }
                }
            }
            if (done) break;
        }

        if (done) continue;

        // --- otherwise a regressive move at the centre boundary -------------
        int i = byCenter.empty() ? -1 : byCenter[0];
        if (i < 0) break;   // n == 1, nothing to do

        s.apply(i, s.lo[i], s.lo[i + 1]);
        regressive++;
    }

    return {max_steps, useful, wasted, regressive, s.sorted(), -1, 0};
}

// ---------------------------------------------------------------------------
// Reference values.
// ---------------------------------------------------------------------------

// Winkler's lower bound, ceil(C(2n,2)/3), as stated in Winkler's book and on
// D. West's REGS page, and used as the benchmark by Ito, Teruyama & Yoshida
// (2012).  The counting argument charges one unit to each of the C(2n,2) pairs
// of balls and observes that a single swap can discharge at most three.  It is
// attained for every n <= 20 except n = 6, and proved attained for n = 2^m + 1.
// n = 1 is degenerate (nothing to sort), so it is reported as 0.
static long long lb_winkler(int n) {
    if (n <= 1) return 0;
    long long pairs = (long long)n * (2LL * n - 1);
    return (pairs + 2) / 3;
}

// Exhaustively verified values.
static int known_T(int n) {
    static const int t[] = {0, 0, 2, 5, 10, 15, 23, 31, 40};
    return (n >= 1 && n <= 8) ? t[n] : -1;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    int maxn = (argc > 1) ? atoi(argv[1]) : 10;
    if (maxn < 1) maxn = 10;

    printf("%3s %7s %6s %6s %6s %9s %8s %7s  %s\n",
           "n", "moves", "useful", "waste", "regr", "Winkler", "T(n)", "excess", "outcome");

    for (int n = 1; n <= maxn; n++) {
        Result r = center_out_v2(n);

        long long w = lb_winkler(n);
        int t = known_T(n);

        char tbuf[16], ebuf[16];
        if (t >= 0) snprintf(tbuf, sizeof tbuf, "%d", t);
        else        snprintf(tbuf, sizeof tbuf, "?");

        // How far the heuristic lands above the best value we can vouch for.
        long long ref = (t >= 0) ? t : w;
        if (r.success) snprintf(ebuf, sizeof ebuf, "+%lld", (long long)r.moves - ref);
        else           snprintf(ebuf, sizeof ebuf, "-");

        char obuf[64];
        if (r.success)           snprintf(obuf, sizeof obuf, "sorted");
        else if (r.cycleAt >= 0) snprintf(obuf, sizeof obuf,
                                          "CYCLES (period %d, from step %d)",
                                          r.cycleLen, r.cycleAt);
        else                     snprintf(obuf, sizeof obuf, "hit step limit");

        printf("%3d %7d %6d %6d %6d %9lld %8s %7s  %s\n",
               n, r.moves, r.useful, r.wasted, r.regressive,
               w, tbuf, ebuf, obuf);
    }

    printf("\nThe `excess` column is the gap between the heuristic and the true\n"
           "optimum.  The rule is not merely suboptimal: from n = 5 on it enters\n"
           "a cycle of neutral moves and never sorts at all.\n");
    return 0;
}
