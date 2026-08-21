#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace std;


// ------------------------------------------------------------
// Return the target bin of a label.
// Labels are 1-indexed; bins are 0-indexed.
// ------------------------------------------------------------
int target_bin_of(int label) {
    return label - 1;
}


// ------------------------------------------------------------
// Determine what kinds of objects are in a bin.
//
// H = object is already in its target bin
// R = object needs to move right
// L = object needs to move left
// ------------------------------------------------------------
set<char> bin_kind(const vector<int>& binc, int i) {
    set<char> types;

    for (int l : binc) {
        int tb = target_bin_of(l);

        if (tb == i) {
            types.insert('H');
        }
        else if (tb > i) {
            types.insert('R');
        }
        else {
            types.insert('L');
        }
    }

    return types;
}


// ------------------------------------------------------------
// Find boundaries where:
//
// left bin contains an R-object
// right bin contains an L-object
//
// Such a boundary allows a useful move.
// ------------------------------------------------------------
vector<int> useful_boundaries(
    const vector<vector<int>>& state,
    int n
) {
    vector<int> res;

    for (int i = 0; i < n - 1; i++) {

        set<char> tA = bin_kind(state[i], i);
        set<char> tB = bin_kind(state[i + 1], i + 1);

        if (tA.count('R') && tB.count('L')) {
            res.push_back(i);
        }
    }

    return res;
}


// ------------------------------------------------------------
// Potential function D.
//
// D = total distance of all objects from their target bins.
// ------------------------------------------------------------
int D(
    const vector<vector<int>>& state,
    int n
) {
    int total = 0;

    for (int i = 0; i < n; i++) {
        for (int l : state[i]) {
            total += abs(i - target_bin_of(l));
        }
    }

    return total;
}


// ------------------------------------------------------------
// Apply an adjacent swap.
//
// Swap object a from bin i
// with object b from bin i+1.
// ------------------------------------------------------------
vector<vector<int>> apply_move(
    const vector<vector<int>>& state,
    int i,
    int a,
    int b
) {
    vector<vector<int>> new_state = state;

    // Remove a from left bin.
    auto itA = find(
        new_state[i].begin(),
        new_state[i].end(),
        a
    );

    if (itA != new_state[i].end()) {
        new_state[i].erase(itA);
    }

    // Remove b from right bin.
    auto itB = find(
        new_state[i + 1].begin(),
        new_state[i + 1].end(),
        b
    );

    if (itB != new_state[i + 1].end()) {
        new_state[i + 1].erase(itB);
    }

    // Swap them.
    new_state[i].push_back(b);
    new_state[i + 1].push_back(a);

    // Keep bins sorted, matching the Python tuple behavior.
    sort(
        new_state[i].begin(),
        new_state[i].end()
    );

    sort(
        new_state[i + 1].begin(),
        new_state[i + 1].end()
    );

    return new_state;
}


// ------------------------------------------------------------
// Check whether two states are equal.
// ------------------------------------------------------------
bool states_equal(
    const vector<vector<int>>& a,
    const vector<vector<int>>& b
) {
    return a == b;
}


// ------------------------------------------------------------
// Main center-out heuristic.
// ------------------------------------------------------------
struct Result {
    int moves;
    int useful;
    int wasted;
    int regressive;
    bool success;
};


Result center_out_v2(
    int n,
    int max_steps = 2000
) {

    // --------------------------------------------------------
    // Initial state:
    //
    // n=4:
    //
    // (4,4)
    // (3,3)
    // (2,2)
    // (1,1)
    // --------------------------------------------------------

    vector<vector<int>> state(n);

    for (int i = 0; i < n; i++) {
        int label = n - i;

        state[i] = {label, label};
    }


    // --------------------------------------------------------
    // Target state:
    //
    // (1,1)
    // (2,2)
    // ...
    // (n,n)
    // --------------------------------------------------------

    vector<vector<int>> target(n);

    for (int i = 0; i < n; i++) {
        int label = i + 1;

        target[i] = {label, label};
    }


    // Center of the boundaries.
    double center = (n - 1) / 2.0;


    int useful_c = 0;
    int wasted_c = 0;
    int regressive_c = 0;


    // --------------------------------------------------------
    // Main simulation loop.
    // --------------------------------------------------------

    for (int step = 0; step < max_steps; step++) {

        // Have we reached the target?
        if (states_equal(state, target)) {
            return {
                step,
                useful_c,
                wasted_c,
                regressive_c,
                true
            };
        }


        // ----------------------------------------------------
        // First try to find a useful boundary.
        // ----------------------------------------------------

        vector<int> avail =
            useful_boundaries(state, n);


        if (!avail.empty()) {

            // Choose the useful boundary closest
            // to the center.
            int best_i = avail[0];

            double best_dist =
                abs((best_i + 0.5) - center);

            for (int x : avail) {

                double dist =
                    abs((x + 0.5) - center);

                if (dist < best_dist) {
                    best_dist = dist;
                    best_i = x;
                }
            }

            int i = best_i;


            const vector<int>& binA = state[i];
            const vector<int>& binB = state[i + 1];


            // Find an object in bin A that wants to move right.
            int a = -1;

            for (int l : binA) {
                if (target_bin_of(l) > i) {
                    a = l;
                    break;
                }
            }


            // Find an object in bin B that wants to move left.
            int b = -1;

            for (int l : binB) {
                if (target_bin_of(l) < i + 1) {
                    b = l;
                    break;
                }
            }


            // Perform useful move.
            state = apply_move(
                state,
                i,
                a,
                b
            );

            useful_c++;
        }


        // ----------------------------------------------------
        // No useful move exists.
        // Look for a wasted move.
        // ----------------------------------------------------

        else {

            vector<int> boundaries_by_dist;

            for (int i = 0; i < n - 1; i++) {
                boundaries_by_dist.push_back(i);
            }


            // Sort boundaries by distance from center.
            sort(
                boundaries_by_dist.begin(),
                boundaries_by_dist.end(),
                [center](int a, int b) {

                    return abs((a + 0.5) - center)
                         < abs((b + 0.5) - center);
                }
            );


            bool done = false;


            // ------------------------------------------------
            // Look for a wasted move:
            //
            // delta D = 0
            // ------------------------------------------------

            for (int i : boundaries_by_dist) {

                const vector<int>& binA = state[i];
                const vector<int>& binB = state[i + 1];

                int d0 = D(state, n);


                bool found = false;


                // Use a set to imitate Python's set(binA).
                set<int> uniqueA(
                    binA.begin(),
                    binA.end()
                );

                set<int> uniqueB(
                    binB.begin(),
                    binB.end()
                );


                for (int a : uniqueA) {

                    for (int b : uniqueB) {

                        vector<vector<int>> ns =
                            apply_move(
                                state,
                                i,
                                a,
                                b
                            );


                        // Ignore moves that do nothing.
                        if (states_equal(ns, state)) {
                            continue;
                        }


                        int d1 = D(ns, n);

                        int delta = d1 - d0;


                        // Wasted move.
                        if (delta == 0) {

                            state = ns;

                            wasted_c++;

                            done = true;

                            found = true;

                            break;
                        }
                    }

                    if (found) {
                        break;
                    }
                }


                if (done) {
                    break;
                }
            }


            // ------------------------------------------------
            // No wasted move exists.
            //
            // Take a regressive move at the nearest boundary.
            // ------------------------------------------------

            if (!done) {

                int i = boundaries_by_dist[0];


                const vector<int>& binA = state[i];
                const vector<int>& binB = state[i + 1];


                int a = binA[0];
                int b = binB[0];


                state = apply_move(
                    state,
                    i,
                    a,
                    b
                );


                regressive_c++;
            }
        }
    }


    // --------------------------------------------------------
    // Maximum number of steps reached.
    // --------------------------------------------------------

    return {
        max_steps,
        useful_c,
        wasted_c,
        regressive_c,
        states_equal(state, target)
    };
}


// ------------------------------------------------------------
// Known optimal values T(n).
// ------------------------------------------------------------

int known_T(int n) {

    switch (n) {
        case 1: return 0;
        case 2: return 2;
        case 3: return 5;
        case 4: return 10;
        case 5: return 15;
        case 6: return 23;
        case 7: return 31;
        default: return -1;
    }
}


// ------------------------------------------------------------
// Main.
// ------------------------------------------------------------

int main() {

    for (int n = 1; n <= 7; n++) {

        Result r =
            center_out_v2(n);


        int true_T = known_T(n);


        bool match =
            r.success &&
            r.moves == true_T;


        cout
            << "n=" << n
            << ": moves=" << r.moves
            << " (u=" << r.useful
            << ",z=" << r.wasted
            << ",p=" << r.regressive
            << ") success=" << boolalpha << r.success
            << " true T(n)=" << true_T
            << " MATCH=" << boolalpha << match
            << '\n';
    }


    return 0;
}