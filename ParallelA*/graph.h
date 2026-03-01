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
    thread_buffers[compute_recipient(start)].send(start);
    threads.reserve(th_n);
    for (int i = 0; i < th_n; i++)
        threads.emplace_back(worker, i, ref(thread_buffers[i]), g);
    for (auto &t : threads)
        t.join();

    return chrono::high_resolution_clock::now();
}