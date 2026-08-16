#include <bits/stdc++.h>
using namespace std;

typedef unsigned __int128 u128;

struct Hash128 {
    size_t operator()(const u128 &x) const {
        uint64_t hi = (uint64_t)(x >> 64);
        uint64_t lo = (uint64_t)(x & ~(uint64_t)0);
        uint64_t h = hi * 1099511628211ULL ^ lo;
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
        return (size_t)h;
    }
};

int n;
int base_; // encoding base for pair (a<=b): a*base_+b, base_ = n+1

u128 encode(vector<int>& bins /* size n, each value = a*base_+b */) {
    u128 x = 0;
    for (int i = 0; i < n; i++) {
        x = (x << 8) | (unsigned)bins[i];
    }
    return x;
}

void decode(u128 x, vector<int>& bins) {
    for (int i = n-1; i >= 0; i--) {
        bins[i] = (int)(x & 0xFF);
        x >>= 8;
    }
}

// generate neighbors of state, call fn(newstate) for each
template<typename F>
void neighbors(u128 state, F fn) {
    vector<int> bins(n);
    decode(state, bins);
    for (int i = 0; i < n-1; i++) {
        int A = bins[i], B = bins[i+1];
        int a1 = A / base_, a2 = A % base_;
        int b1 = B / base_, b2 = B % base_;
        int avals[2] = {a1, a2};
        int bvals[2] = {b1, b2};
        set<pair<int,int>> seen;
        for (int ai = 0; ai < 2; ai++) {
            for (int bi = 0; bi < 2; bi++) {
                int a = avals[ai], b = bvals[bi];
                if (seen.count({a,b})) continue;
                seen.insert({a,b});
                // new bin i: remove one 'a', add 'b'
                int na1 = (ai==0)? b : a1;
                int na2 = (ai==1)? b : a2;
                int nb1 = (bi==0)? a : b1;
                int nb2 = (bi==1)? a : b2;
                if (na1 > na2) swap(na1,na2);
                if (nb1 > nb2) swap(nb1,nb2);
                int newA = na1*base_+na2;
                int newB = nb1*base_+nb2;
                if (newA == A && newB == B) continue;
                vector<int> nb2v = bins;
                nb2v[i] = newA; nb2v[i+1] = newB;
                u128 ns = encode(nb2v);
                fn(ns);
            }
        }
    }
}

int main(int argc, char** argv) {
    n = atoi(argv[1]);
    base_ = n+1;
    vector<int> startb(n), targetb(n);
    for (int i = 1; i <= n; i++) {
        int lbl = n+1-i;
        startb[i-1] = lbl*base_+lbl;
        targetb[i-1] = i*base_+i;
    }
    u128 start = encode(startb);
    u128 target = encode(targetb);
    if (start == target) { printf("%d 0\n", n); return 0; }

    unordered_map<u128,int,Hash128> distS, distT;
    distS[start]=0; distT[target]=0;
    vector<u128> curS = {start}, curT = {target};
    int ans = -1;
    int depth=0;
    auto t0 = chrono::steady_clock::now();
    while (!curS.empty() && !curT.empty()) {
        bool expandS = curS.size() <= curT.size();
        vector<u128>& cur = expandS ? curS : curT;
        auto& distCur = expandS ? distS : distT;
        auto& distOther = expandS ? distT : distS;
        vector<u128> next;
        int d = expandS ? (distS[cur[0]]+1) : (distT[cur[0]]+1) ;
        bool found=false;
        for (u128 s : cur) {
            int dd = distCur[s];
            neighbors(s, [&](u128 ns){
                if (found) return;
                auto itO = distOther.find(ns);
                if (itO != distOther.end()) {
                    ans = dd+1+itO->second;
                    found = true;
                    return;
                }
                if (distCur.find(ns) == distCur.end()) {
                    distCur[ns] = dd+1;
                    next.push_back(ns);
                }
            });
            if (found) break;
        }
        if (found) break;
        cur = next;
        double elapsed = chrono::duration<double>(chrono::steady_clock::now()-t0).count();
        fprintf(stderr, "n=%d depth~ front sizes S=%zu T=%zu elapsed=%.1f\n", n, distS.size(), distT.size(), elapsed);
        if (elapsed > 1800) { printf("%d TIMEOUT\n", n); return 0; }
    }
    printf("%d %d\n", n, ans);
    return 0;
}