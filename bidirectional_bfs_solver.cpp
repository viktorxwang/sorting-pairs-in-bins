// ---------------------------------------------------------------------------
// bidirectional_bfs_solver.cpp
//
// Independent cross-check of T(n) for Winkler's "Sorting Pairs in Bins".
//
// This deliberately uses a different design from sortpairs.cpp -- a hash table
// over packed states rather than a perfect-hash byte array -- so that agreement
// between the two is meaningful evidence.  It is the slower of the two and is
// limited by memory to n <= 7; use sortpairs.cpp for n = 8.
//
// Fixed in this revision:
//
//   * Correctness: the previous version stopped at the *first* frontier state
//     found in the opposite search's table and reported that meeting's total.
//     Frontier states all sit at one depth, but the states they meet do not, so
//     the first meeting need not be the cheapest.  It now finishes the level,
//     takes the minimum over all meetings, and only stops once that minimum is
//     provably unbeatable (best <= depthS + depthT).
//
//   * Speed: neighbour generation no longer allocates a vector and a std::set
//     per boundary per state.
//
// Build:
//   g++ -O3 -march=native -DNDEBUG -o bidi.exe bidirectional_bfs_solver.cpp
// ---------------------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// ---------------------------------------------------------------------------
// 128-bit state word: eight bits per bin, each holding a1*base + a2 for the
// bin's sorted pair.  Portable so this file builds with any C++ compiler.
// ---------------------------------------------------------------------------
struct u128 {
    uint64_t hi, lo;

    u128(uint64_t h = 0, uint64_t l = 0) : hi(h), lo(l) {}

    bool operator==(const u128& x) const { return hi == x.hi && lo == x.lo; }
    bool operator!=(const u128& x) const { return !(*this == x); }
};

struct Hash128 {
    size_t operator()(const u128& x) const {
        uint64_t h = x.hi * 1099511628211ULL ^ x.lo;
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;
        return (size_t)h;
    }
};

static int n;
static int base_;

// Bins are packed low-to-high so encode/decode need no shifting loop over a
// 128-bit value; bin i lives in byte i.
static inline u128 encode(const int* bins) {
    uint64_t lo = 0, hi = 0;
    for (int i = 0; i < n && i < 8; i++)
        lo |= (uint64_t)(unsigned char)bins[i] << (8 * i);
    for (int i = 8; i < n; i++)
        hi |= (uint64_t)(unsigned char)bins[i] << (8 * (i - 8));
    return u128(hi, lo);
}

static inline void decode(const u128& x, int* bins) {
    for (int i = 0; i < n && i < 8; i++)
        bins[i] = (int)((x.lo >> (8 * i)) & 0xFF);
    for (int i = 8; i < n; i++)
        bins[i] = (int)((x.hi >> (8 * (i - 8))) & 0xFF);
}

// ---------------------------------------------------------------------------
// Successors of a state.  Each bin holds a sorted pair (a1 <= a2) packed as
// a1*base + a2; a move swaps one ball across one boundary.
// ---------------------------------------------------------------------------
template <typename F>
static void neighbors(const u128& state, int* bins, F fn) {
    decode(state, bins);

    for (int i = 0; i + 1 < n; i++) {
        int A = bins[i], B = bins[i + 1];

        int a1 = A / base_, a2 = A % base_;
        int b1 = B / base_, b2 = B % base_;

        int avals[2] = {a1, a2};
        int bvals[2] = {b1, b2};

        // A matched pair offers only one distinct choice, which is all the
        // deduplication this needs -- no std::set required.
        int na = (a1 == a2) ? 1 : 2;
        int nb = (b1 == b2) ? 1 : 2;

        for (int ai = 0; ai < na; ai++) {
            for (int bi = 0; bi < nb; bi++) {
                int give = avals[ai];       // leaves bin i
                int recv = bvals[bi];       // leaves bin i+1
                if (give == recv) continue; // identity move

                int keepA = avals[1 - ai];
                int keepB = bvals[1 - bi];

                int nA = min(keepA, recv) * base_ + max(keepA, recv);
                int nB = min(keepB, give) * base_ + max(keepB, give);

                bins[i] = nA;
                bins[i + 1] = nB;
                fn(encode(bins));
                bins[i] = A;
                bins[i + 1] = B;
            }
        }
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <n> [timeout_seconds]\n", argv[0]);
        return 2;
    }

    n = atoi(argv[1]);
    if (n < 1 || n > 16) {
        fprintf(stderr, "n must be between 1 and 16\n");
        return 2;
    }

    double timeout = (argc > 2) ? atof(argv[2]) : 1800.0;
    base_ = n + 1;

    vector<int> startb(n), targetb(n), scratch(n);

    for (int i = 1; i <= n; i++) {
        int lbl = n + 1 - i;
        startb[i - 1]  = lbl * base_ + lbl;
        targetb[i - 1] = i * base_ + i;
    }

    u128 start  = encode(startb.data());
    u128 target = encode(targetb.data());

    if (start == target) {
        printf("%d 0\n", n);
        return 0;
    }

    unordered_map<u128, int, Hash128> distS, distT;
    distS.max_load_factor(0.7f);
    distT.max_load_factor(0.7f);
    distS[start] = 0;
    distT[target] = 0;

    vector<u128> curS = {start}, curT = {target};

    int depthS = 0, depthT = 0;
    int best = INT32_MAX;

    auto t0 = chrono::steady_clock::now();

    while (!curS.empty() && !curT.empty()) {
        // Always grow the cheaper side.
        bool expandS = curS.size() <= curT.size();

        vector<u128>& cur       = expandS ? curS  : curT;
        auto&          distCur  = expandS ? distS : distT;
        auto&          distOther= expandS ? distT : distS;
        int&           depthCur = expandS ? depthS : depthT;

        int dd = depthCur;
        vector<u128> next;
        next.reserve(cur.size() * 4);

        // Expand the WHOLE level before deciding anything: frontier states all
        // sit at depth dd, but the states they meet on the other side do not,
        // so the cheapest meeting can appear anywhere in the level.
        for (const u128& s : cur) {
            neighbors(s, scratch.data(), [&](u128 ns) {
                auto itO = distOther.find(ns);
                if (itO != distOther.end())
                    best = min(best, dd + 1 + itO->second);

                if (distCur.emplace(ns, dd + 1).second)
                    next.push_back(ns);
            });
        }

        cur = next;
        depthCur = dd + 1;

        // Any remaining path must cost at least depthS + depthT, so once best
        // has reached that floor it cannot be improved.
        if (best <= depthS + depthT) break;

        double elapsed =
            chrono::duration<double>(chrono::steady_clock::now() - t0).count();

        fprintf(stderr,
                "n=%d depthS=%d depthT=%d |S|=%zu |T|=%zu best=%s elapsed=%.1fs\n",
                n, depthS, depthT, distS.size(), distT.size(),
                best == INT32_MAX ? "-" : to_string(best).c_str(), elapsed);

        if (elapsed > timeout) {
            printf("%d TIMEOUT\n", n);
            return 0;
        }
    }

    if (best == INT32_MAX) {
        printf("%d UNREACHABLE\n", n);
        return 1;
    }

    printf("%d %d\n", n, best);
    return 0;
}
