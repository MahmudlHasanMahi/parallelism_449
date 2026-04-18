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
#include <mpi.h>
#include "ParallelA*/graph.h"
using namespace std;

int main(int argc, char **argv)
{
    // ── MPI init ────────────────────────────────
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE)
    {
        cout << "ERROR: MPI does not support MPI_THREAD_MULTIPLE" << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &my_machine);
    MPI_Comm_size(MPI_COMM_WORLD, &num_machines);
    // th_n = 4;
    for (int i = 0; i < th_n; i++)
        thread_buffers.push_back(make_unique<thread_buffer>());
    // ── setup (all machines do this) ────────────
    int S = 1, G = 25201;
    string FILE_NAME = "big_graph.txt";

    // every machine loads the graph — they all need it for get_successor
    ifstream file(FILE_NAME);
    int u, v, w;
    while (file >> u >> v >> w)
        adj[u].push_back({v, w});
    cout << "graph generated" << endl;
    // wait for all machines to finish loading before starting
    MPI_Barrier(MPI_COMM_WORLD);

    // ── inject start node ───────────────────────
    // only machine 0 seeds the start node
    if (my_machine == 0)
    {
        Node start;
        start.id = S;
        cout << "machine 0 seeding start node " << start.id << endl;
        send_node(start);
    }

    // ── launch threads ──────────────────────────
    auto start_time = chrono::high_resolution_clock::now();

    threads.reserve(th_n);
    for (int i = 0; i < th_n; i++)
        threads.emplace_back(worker, i, ref(*thread_buffers[i]), G);

    // network receiver — handles incoming MPI node messages
    thread receiver(network_receiver);

    // FIX: All machines must run the termination coordinator
    thread coordinator(termination_coordinator);

    if (my_machine == 0)
    {
        // small sleep to make sure all workers are in their loop
        this_thread::sleep_for(chrono::milliseconds(100));
        Node start;
        start.id = S;
        start.g = 0;
        start.h = 0;
        cout << "machine 0 seeding start node " << start.id << endl;
        send_node(start);
    }

    for (auto &t : threads)
        t.join();

    receiver.join();
    coordinator.join();

    auto end_time = chrono::high_resolution_clock::now();

    // ── collect results ─────────────────────────
    // gathers global_state from all machines into machine 0
    collect_global_state();

    // sync before printing so machine 0 has everything
    MPI_Barrier(MPI_COMM_WORLD);

    // ── print results — only machine 0 ──────────
    if (my_machine == 0)
    {
        vector<int> path;
        int cur = best_node;
        while (cur != -1)
        {
            path.push_back(cur);
            cur = global_state.count(cur) ? global_state[cur].parent : -1;
        }
        reverse(path.begin(), path.end());

        cout << endl;
        cout << "    C++ PERFORMANCE OF A*" << endl;
        cout << "    NUMBER OF THREADS: " << th_n << " x " << num_machines << " machines" << endl;
        cout << "    " << left << setw(14) << "Time" << ": " << chrono::duration<double>(end_time - start_time).count() << " seconds" << endl;
        cout << "    " << left << setw(14) << "Source" << ": " << S << endl;
        cout << "    " << left << setw(14) << "Destination" << ": " << G << endl;
        cout << "    " << left << setw(14) << "Best cost" << ": " << best_cost << endl;
        cout << "    " << left << setw(14) << "Path nodes" << ": " << path.size() + 1 << endl;
        cout << "    " << left << setw(14) << "Node Explored" << ": " << nodecount << endl;
        cout << "    " << left << setw(14) << "Path" << ": " << endl;
        for (int i = 0; i < (int)path.size(); i++)
        {
            if (i)
                cout << " ->";
            cout << path[i];
        }
        cout << endl;
    }

    MPI_Finalize();
    return 0;
}
// int main()
// {
//     th_n = 4;
//     int S = 1, G = 10000000;

//     string FILE_NAME = "big_graph.txt";
//     ifstream file(FILE_NAME);
//     int u, v, w;

//     while (file >> u >> v >> w)
//     {
//         adj[u].push_back({v, w});
//     }

//     auto start = chrono::high_resolution_clock::now();
//     auto end = ga_star_process(S, G);

//     vector<int> path;
//     int cur = best_node;
//     while (cur != -1)
//     {
//         path.push_back(cur);
//         cur = global_state.count(cur) ? global_state[cur].parent : -1;
//     }
//     reverse(path.begin(), path.end());

//     // Print
//     cout << endl;
//     cout << "    C++ PERFORMANCE OF A*" << endl;
//     cout << "    NUMBER OF THREADS: " << th_n << endl;
//     cout << "    " << left << setw(14) << "Time" << ": " << chrono::duration<double>(end - start).count() << " seconds" << endl;
//     cout << "    " << left << setw(14) << "Source" << ": " << S << endl;
//     cout << "    " << left << setw(14) << "Destination" << ": " << G << endl;
//     cout << "    " << left << setw(14) << "Best cost" << ": " << best_cost << endl;
//     cout << "    " << left << setw(14) << "Path nodes" << ": " << path.size() + 1 << endl;
//     cout << "    " << left << setw(14) << "Node Explored" << ": " << nodecount << endl;
//     ;
//     cout << "    " << left << setw(14) << "Path" << ": " << endl;
//     for (int i = 0; i < (int)path.size(); i++)
//     {
//         if (i)
//             cout << " ->";
//         cout << path[i];
//     }
//     cout << endl;

//     return 0;
// }