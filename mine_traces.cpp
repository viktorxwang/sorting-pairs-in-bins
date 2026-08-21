#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

using State = vector<vector<int>>;

struct Move {
    int boundary;
    int a;
    int b;
};


// ------------------------------------------------------------
// Return the target bin of a label.
// Labels are 1-indexed; bins are 0-indexed.
// ------------------------------------------------------------
int target_bin_of(int label) {
    return label - 1;
}


// ------------------------------------------------------------
// Find an optimal path from the reversed state to the target.
//
// Uses BFS, so the first path found is guaranteed to be
// a shortest path.
// ------------------------------------------------------------
vector<Move> solve_path(int n) {

    // Starting state:
    //
    // n=4:
    // (4,4) (3,3) (2,2) (1,1)

    State start(n);

    for (int i = 0; i < n; i++) {
        int label = n - i;
        start[i] = {label, label};
    }


    // Target:
    //
    // (1,1) (2,2) (3,3) (4,4)

    State target(n);

    for (int i = 0; i < n; i++) {
        int label = i + 1;
        target[i] = {label, label};
    }


    if (start == target) {
        return {};
    }


    // BFS queue.
    deque<State> q;
    q.push_back(start);


    // Parent information:
    //
    // parent[state] = {previous_state, boundary, a, b}
    //
    // We use a map because vector<vector<int>> is not directly
    // hashable in standard C++17.

    struct ParentInfo {
        bool has_parent;
        State parent;
        int boundary;
        int a;
        int b;
    };

    map<State, ParentInfo> visited;

    visited[start] = {
        false,
        {},
        -1,
        -1,
        -1
    };


    while (!q.empty()) {

        State state = q.front();
        q.pop_front();


        // ----------------------------------------------------
        // Try every adjacent boundary.
        // ----------------------------------------------------

        for (int i = 0; i < n - 1; i++) {

            vector<int> binA = state[i];
            vector<int> binB = state[i + 1];


            // The Python script uses a set of (a,b) pairs
            // to avoid duplicate swaps.
            set<pair<int, int>> seen;


            // Each bin contains exactly two objects.
            for (int ia = 0; ia < 2; ia++) {

                for (int ib = 0; ib < 2; ib++) {

                    int a = binA[ia];
                    int b = binB[ib];


                    if (seen.count({a, b})) {
                        continue;
                    }

                    seen.insert({a, b});


                    // Make copies of the two bins.
                    vector<int> newA = binA;
                    vector<int> newB = binB;


                    // Swap the selected objects.
                    newA[ia] = b;
                    newB[ib] = a;


                    // Construct new state.
                    State ns = state;

                    sort(newA.begin(), newA.end());
                    sort(newB.begin(), newB.end());

                    ns[i] = newA;
                    ns[i + 1] = newB;


                    // Ignore moves that leave the state unchanged.
                    if (ns == state) {
                        continue;
                    }


                    // Have we visited this state?
                    if (visited.find(ns) == visited.end()) {

                        visited[ns] = {
                            true,
                            state,
                            i,
                            a,
                            b
                        };


                        // Target reached.
                        if (ns == target) {

                            vector<Move> path;

                            State cur = ns;


                            // Follow parents backwards.
                            while (visited[cur].has_parent) {

                                ParentInfo info =
                                    visited[cur];

                                path.push_back({
                                    info.boundary,
                                    info.a,
                                    info.b
                                });

                                cur = info.parent;
                            }


                            // Reverse to get start -> target order.
                            reverse(
                                path.begin(),
                                path.end()
                            );


                            return path;
                        }


                        q.push_back(ns);
                    }
                }
            }
        }
    }


    // No solution found.
    return {};
}


// ------------------------------------------------------------
// Potential function D.
//
// D = sum of distances of all objects from their target bins.
// ------------------------------------------------------------
int D(const State& state) {

    int total = 0;

    for (int i = 0; i < (int)state.size(); i++) {

        for (int l : state[i]) {

            total += abs(
                i - target_bin_of(l)
            );
        }
    }

    return total;
}


// ------------------------------------------------------------
// Determine the type(s) of objects in a bin.
//
// H = home
// R = wants to move right
// L = wants to move left
//
// If both objects have the same type:
//
//     RR
//     LL
//     HH
//
// If they have different types:
//
//     LR
//     HL
//     HR
//
// The result is alphabetically sorted, matching Python's
// ''.join(sorted(types)) behavior.
// ------------------------------------------------------------
string bin_type(
    const vector<int>& binc,
    int i
) {

    set<char> types;


    for (int l : binc) {

        int tb =
            target_bin_of(l);


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


    // If both objects have the same type,
    // duplicate the character.

    if (types.size() == 1) {

        char c = *types.begin();

        return string(2, c);
    }


    // Otherwise concatenate sorted types.
    string result;

    for (char c : types) {
        result += c;
    }

    return result;
}


// ------------------------------------------------------------
// Return the H/R/L profile of the entire state.
// ------------------------------------------------------------
string print_profile(
    const State& state,
    int n
) {

    string result;


    for (int i = 0; i < n; i++) {

        if (i > 0) {
            result += " ";
        }

        result += bin_type(
            state[i],
            i
        );
    }


    return result;
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {

    // Same values as the Python script.
    vector<int> values = {
        4, 5, 6, 7
    };


    for (int n : values) {

        // Starting state.
        State start(n);

        for (int i = 0; i < n; i++) {

            int label = n - i;

            start[i] = {
                label,
                label
            };
        }


        // Find optimal path.
        vector<Move> path =
            solve_path(n);


        State state = start;


        cout << "\n"
             << string(70, '=')
             << "\n";

        cout << "=== n="
             << n
             << "  total_moves="
             << path.size()
             << " ===\n";


        cout << "  t=0 profile: "
             << print_profile(state, n)
             << "\n";


        // ----------------------------------------------------
        // Replay the optimal path.
        // ----------------------------------------------------

        for (int idx = 0;
             idx < (int)path.size();
             idx++) {

            int i = path[idx].boundary;
            int a = path[idx].a;
            int b = path[idx].b;


            // D before move.
            int d_before =
                D(state);


            // Get the two adjacent bins.
            vector<int> binA =
                state[i];

            vector<int> binB =
                state[i + 1];


            // Remove a from bin A.
            auto itA =
                find(
                    binA.begin(),
                    binA.end(),
                    a
                );

            if (itA != binA.end()) {
                binA.erase(itA);
            }


            // Remove b from bin B.
            auto itB =
                find(
                    binB.begin(),
                    binB.end(),
                    b
                );

            if (itB != binB.end()) {
                binB.erase(itB);
            }


            // Perform swap.
            binA.push_back(b);
            binB.push_back(a);


            // Keep bins sorted.
            sort(
                binA.begin(),
                binA.end()
            );

            sort(
                binB.begin(),
                binB.end()
            );


            // Update state.
            state[i] =
                binA;

            state[i + 1] =
                binB;


            // D after move.
            int d_after =
                D(state);


            int delta =
                d_after - d_before;


            // ------------------------------------------------
            // Only print wasted moves, matching Python.
            // ------------------------------------------------

            if (delta == 0) {

                cout
                    << "  move#"
                    << idx + 1
                    << " boundary"
                    << i
                    << " WASTED "
                    << "(swap "
                    << a
                    << "<->"
                    << b
                    << ")\n";


                cout
                    << "      profile AFTER: "
                    << print_profile(
                        state,
                        n
                    )
                    << "\n";
            }
        }
    }


    return 0;
}