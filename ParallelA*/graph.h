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
#include "worker.h"
using namespace std;

auto ga_star_process(int s, int g)
{
    Node start;
    start.id = s;
    send_node(start);

    threads.reserve(th_n);
    for (int i = 0; i < th_n; i++)
        threads.emplace_back(worker, i, ref(*thread_buffers[i]), g);

    // ← ADD THESE
    thread receiver(network_receiver);

    thread coordinator;
    if (my_machine == 0)
        coordinator = thread(termination_coordinator);

    for (auto &t : threads)
        t.join();
    receiver.join();
    if (my_machine == 0)
        coordinator.join();
    // ← END ADD

    return chrono::high_resolution_clock::now();
}