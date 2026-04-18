
#pragma once
#include <queue>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include "struct.h"

int nodecount = 0;
int th_n = 4;
int my_machine = 0;
int num_machines = 1;

const int TAG_NODE = 1;
const int TAG_BEST_COST = 2;
const int TAG_TERMINATE = 3;

unordered_map<int, vector<pair<int, int>>> adj;
vector<thread> threads;
// vector<thread_buffer> thread_buffers(th_n);
// vector<thread_buffer> thread_buffers;
vector<unique_ptr<thread_buffer>> thread_buffers;
mutex global_mtx, best_cost_mtx;
unordered_map<int, Node> global_state;
int best_cost = numeric_limits<int>::max();
int best_node = -1;
atomic<int> idle_count(0);
atomic<bool> done(false);