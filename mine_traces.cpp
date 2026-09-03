// ---------------------------------------------------------------------------
// mine_traces.cpp
//
// Replays an optimal solution of "Sorting Pairs in Bins" and reports the
// structure of its non-progressing moves: where the neutral ("wasted") moves
// fall, and what the home/right/left type profile of the bins looks like around
// them.  This is the evidence behind the priming-then-useful pattern and the
// type-preservation lemma.
//
// Rewritten for speed and reach.  The previous version keyed BFS on
// std::map<std::vector<std::vector<int>>>, which costs a tree lookup and
// several heap allocations per state and stores the parent as a full nested
// vector; that is on the order of a hundred bytes and a cache miss per state,
// and it does not get through n = 7.  States are now packed into a single
// uint64_t (one nibble per ball, so n <= 8) and held in an open hash map, with
// parents stored as packed states plus the move.
//
// For n = 8 the reachable space is 545,007,960 states and even this does not
// fit; pass a move sequence produced by `sortpairs exact 8` instead:
//
//     sortpairs.exe exact 8 --save path_n8.txt
//     mine_traces.exe --path 8 path_n8.txt
//
// Build:
//   g++ -O3 -march=native -DNDEBUG -o mine_traces.exe mine_traces.cpp
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
typedef uint64_t u64;

static int n;

// ---------------------------------------------------------------------------
// Packed state: bin i occupies byte i, holding (lo << 4) | hi with labels 1..8.
// ---------------------------------------------------------------------------
static inline u64 pack(const int* lo, const int* hi) {
    u64 s = 0;
    for (int i = 0; i < n; i++)
        s |= (u64)(((lo[i] & 0xF) << 4) | (hi[i] & 0xF)) << (8 * i);
    return s;
}

static inline void unpack(u64 s, int* lo, int* hi) {
    for (int i = 0; i < n; i++) {
        int byte = (int)((s >> (8 * i)) & 0xFF);
        lo[i] = byte >> 4;
        hi[i] = byte & 0xF;
    }
}

struct Move {
    int boundary;  // 0-based boundary between bins boundary and boundary+1
    int give;      // label sent right, from bin `boundary`
    int recv;      // label sent left,  from bin `boundary`+1
};

// ---------------------------------------------------------------------------
// Analysis helpers.
// ---------------------------------------------------------------------------

// Labels are 1-indexed, bins 0-indexed, so label l belongs in bin l-1.
static inline int target_bin_of(int label) { return label - 1; }

// Total displacement.  A move changes this by exactly -2, 0 or +2, which is
// what separates useful, neutral and regressive moves.
static int D(const int* lo, const int* hi) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += abs(i - target_bin_of(lo[i]));
        total += abs(i - target_bin_of(hi[i]));
    }
    return total;
}

// Type of a bin: H = at home, R = wants to move right, L = wants to move left.
// Two balls of the same type give HH / RR / LL; otherwise the two letters are
// reported in alphabetical order.
static string bin_type(int lo, int hi, int i) {
    char t[2];
    int  bal[2] = {lo, hi};

    for (int k = 0; k < 2; k++) {
        int tb = target_bin_of(bal[k]);
        t[k] = (tb == i) ? 'H' : (tb > i ? 'R' : 'L');
    }

    if (t[0] > t[1]) swap(t[0], t[1]);
    return string(t, t + 2);
}

static string profile(const int* lo, const int* hi) {
    string r;
    for (int i = 0; i < n; i++) {
        if (i) r += " ";
        r += bin_type(lo[i], hi[i], i);
    }
    return r;
}

static string config(const int* lo, const int* hi) {
    string r;
    char buf[16];
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof buf, "(%d%d)", lo[i], hi[i]);
        r += buf;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Breadth-first search for one optimal path.
// ---------------------------------------------------------------------------
struct Parent {
    u64 prev;
    Move mv;
};

static bool solve_path(vector<Move>& path) {
    vector<int> lo(n), hi(n);

    for (int i = 0; i < n; i++) lo[i] = hi[i] = n - i;
    u64 start = pack(lo.data(), hi.data());

    for (int i = 0; i < n; i++) lo[i] = hi[i] = i + 1;
    u64 target = pack(lo.data(), hi.data());

    path.clear();
    if (start == target) return true;

    unordered_map<u64, Parent> seen;
    seen.max_load_factor(0.7f);
    seen.reserve(1 << 20);
    seen[start] = Parent{start, Move{-1, -1, -1}};

    vector<u64> cur{start}, next;

    while (!cur.empty()) {
        next.clear();

        for (u64 s : cur) {
            unpack(s, lo.data(), hi.data());

            for (int i = 0; i + 1 < n; i++) {
                int p = lo[i],     q = hi[i];
                int u = lo[i + 1], v = hi[i + 1];

                int lpick[2] = {p, q}, rpick[2] = {u, v};
                int nl = (p == q) ? 1 : 2;
                int nr = (u == v) ? 1 : 2;

                for (int li = 0; li < nl; li++) {
                    for (int ri = 0; ri < nr; ri++) {
                        int give = lpick[li], recv = rpick[ri];
                        if (give == recv) continue;   // identity move

                        int keepL = lpick[1 - li], keepR = rpick[1 - ri];

                        int saveLo0 = lo[i],     saveHi0 = hi[i];
                        int saveLo1 = lo[i + 1], saveHi1 = hi[i + 1];

                        lo[i]     = min(keepL, recv);  hi[i]     = max(keepL, recv);
                        lo[i + 1] = min(keepR, give);  hi[i + 1] = max(keepR, give);

                        u64 ns = pack(lo.data(), hi.data());

                        lo[i] = saveLo0;     hi[i] = saveHi0;
                        lo[i + 1] = saveLo1; hi[i + 1] = saveHi1;

                        if (!seen.emplace(ns, Parent{s, Move{i, give, recv}}).second)
                            continue;

                        if (ns == target) {
                            // Walk the parent chain back to the start.
                            for (u64 c = target; c != start; ) {
                                const Parent& pr = seen[c];
                                path.push_back(pr.mv);
                                c = pr.prev;
                            }
                            reverse(path.begin(), path.end());
                            return true;
                        }

                        next.push_back(ns);
                    }
                }
            }
        }

        cur.swap(next);
    }

    return false;
}

// ---------------------------------------------------------------------------
static bool load_path(const char* file, vector<Move>& path) {
    FILE* f = fopen(file, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", file); return false; }

    path.clear();
    int b, g, r;
    while (fscanf(f, "%d %d %d", &b, &g, &r) == 3)
        path.push_back(Move{b, g, r});

    fclose(f);
    return true;
}

static void analyse(const vector<Move>& path) {
    vector<int> lo(n), hi(n);
    for (int i = 0; i < n; i++) lo[i] = hi[i] = n - i;

    printf("\n%s\n", string(74, '=').c_str());
    printf("=== n=%d  total_moves=%zu ===\n", n, path.size());
    printf("  t=0 profile: %s\n", profile(lo.data(), hi.data()).c_str());

    int useful = 0, neutral = 0, regressive = 0;
    int firstUseful = -1, lastNeutral = -1;

    for (size_t k = 0; k < path.size(); k++) {
        int i = path[k].boundary;
        int before = D(lo.data(), hi.data());

        // Apply, validating that the named balls are actually present.
        int keepL, keepR;
        if      (lo[i] == path[k].give) keepL = hi[i];
        else if (hi[i] == path[k].give) keepL = lo[i];
        else { printf("  !! move %zu: label %d not in bin %d\n", k + 1, path[k].give, i); return; }

        if      (lo[i + 1] == path[k].recv) keepR = hi[i + 1];
        else if (hi[i + 1] == path[k].recv) keepR = lo[i + 1];
        else { printf("  !! move %zu: label %d not in bin %d\n", k + 1, path[k].recv, i + 1); return; }

        lo[i]     = min(keepL, path[k].recv);  hi[i]     = max(keepL, path[k].recv);
        lo[i + 1] = min(keepR, path[k].give);  hi[i + 1] = max(keepR, path[k].give);

        int after = D(lo.data(), hi.data());
        int delta = after - before;

        if (delta < 0) { useful++; if (firstUseful < 0) firstUseful = (int)k; }
        else if (delta == 0) { neutral++; lastNeutral = (int)k; }
        else regressive++;

        if (delta >= 0) {
            printf("  move#%-3zu boundary%-2d %-10s (swap %d<->%d)\n",
                   k + 1, i, delta == 0 ? "NEUTRAL" : "REGRESSIVE",
                   path[k].give, path[k].recv);
            printf("      config  AFTER: %s\n", config(lo.data(), hi.data()).c_str());
            printf("      profile AFTER: %s\n", profile(lo.data(), hi.data()).c_str());
        }
    }

    vector<int> tlo(n), thi(n);
    for (int i = 0; i < n; i++) tlo[i] = thi[i] = i + 1;
    bool ok = !memcmp(lo.data(), tlo.data(), sizeof(int) * n) &&
              !memcmp(hi.data(), thi.data(), sizeof(int) * n);

    printf("\n  useful=%d neutral=%d regressive=%d   total=%zu\n",
           useful, neutral, regressive, path.size());
    printf("  accounting: useful-regressive=%d, floor(n^2/2)=%d  [%s]\n",
           useful - regressive, n * n / 2,
           (useful - regressive == n * n / 2) ? "consistent" : "INCONSISTENT");
    printf("  first useful move at #%d, last non-useful move at #%d\n",
           firstUseful + 1, lastNeutral + 1);
    printf("  reached target: %s\n", ok ? "yes" : "NO");
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    vector<int> sizes;
    const char* pathFile = nullptr;

    if (argc >= 4 && string(argv[1]) == "--path") {
        n = atoi(argv[2]);
        pathFile = argv[3];
        sizes.push_back(n);
    } else if (argc >= 2) {
        for (int k = 1; k < argc; k++) sizes.push_back(atoi(argv[k]));
    } else {
        sizes = {4, 5, 6, 7};
    }

    for (int sz : sizes) {
        if (sz < 1 || sz > 8) {
            fprintf(stderr, "n=%d out of range (this encoding supports n <= 8)\n", sz);
            continue;
        }
        n = sz;

        vector<Move> path;

        if (pathFile) {
            if (!load_path(pathFile, path)) return 1;
        } else if (n >= 8) {
            fprintf(stderr,
                    "n=8 has 545,007,960 states; run `sortpairs exact 8 --save path_n8.txt`\n"
                    "then `mine_traces --path 8 path_n8.txt`\n");
            continue;
        } else if (!solve_path(path)) {
            fprintf(stderr, "no path found for n=%d\n", n);
            continue;
        }

        analyse(path);
    }

    return 0;
}
