#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include "ParallelA*/graph.h"
using namespace std;

int main()
{
    th_n = 4;
    int S = 1, G = 10000000;

    string FILE_NAME = "big_graph.txt";
    ifstream file(FILE_NAME);
    int u, v, w;

    while (file >> u >> v >> w)
    {
        adj[u].push_back({v, w});
    }

    auto start = chrono::high_resolution_clock::now();
    auto end = ga_star_process(S, G);

    vector<int> path;
    int cur = best_node;
    while (cur != -1)
    {
        path.push_back(cur);
        cur = global_state.count(cur) ? global_state[cur].parent : -1;
    }
    reverse(path.begin(), path.end());

    // Print
    cout << endl;
    cout << "    C++ PERFORMANCE OF A*" << endl;
    cout << "    NUMBER OF THREADS: " << th_n << endl;
    cout << "    " << left << setw(14) << "Time" << ": " << chrono::duration<double>(end - start).count() << " seconds" << endl;
    cout << "    " << left << setw(14) << "Source" << ": " << S << endl;
    cout << "    " << left << setw(14) << "Destination" << ": " << G << endl;
    cout << "    " << left << setw(14) << "Best cost" << ": " << best_cost << endl;
    cout << "    " << left << setw(14) << "Path nodes" << ": " << path.size() + 1 << endl;
    cout << "    " << left << setw(14) << "Node Explored" << ": " << nodecount << endl;
    ;
    cout << "    " << left << setw(14) << "Path" << ": " << endl;
    for (int i = 0; i < (int)path.size(); i++)
    {
        if (i)
            cout << " ->";
        cout << path[i];
    }
    cout << endl;

    return 0;
}