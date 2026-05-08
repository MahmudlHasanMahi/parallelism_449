#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include "struct.h"
#include "global.h"
using namespace std;

vector<Node> get_successor(Node &parent)
{
    vector<Node> neighbors;
    for (auto &e : adj[parent.id])
    {
        int v = e.first, cost = e.second;
        if (cost > 0 && (global_state.find(v) == global_state.end() || parent.g + cost < global_state[v].g))
        {
            neighbors.push_back({v, parent.g + cost, 0, parent.id});
        }
    }
    return neighbors;
}

bool termination()
{
    if (idle_count.load() < th_n)
        return false;
    for (auto &tb : thread_buffers)
    {
        if (!tb.empty())
            return false;
    }
    return true;
}

int compute_recipient(Node n) { return n.id % th_n; }
int zobrist_hash(Node n)
{
    return 0;
}

void worker(int id, thread_buffer &th_buff, int goal)
{
    priority_queue<Node, vector<Node>, CompareNode> open_list;
    unordered_set<int> open_set, closed_set;
    unordered_map<int, int> open_g;
    bool is_idle = false;
    while (!done.load())
    {
        Node nb;
        while (th_buff.try_receive(nb))
        {
            if (closed_set.count(nb.id))
            {
                if (nb.g < global_state[nb.id].g)
                {
                    closed_set.erase(nb.id);
                    open_list.push(nb);
                    open_set.insert(nb.id);
                    open_g[nb.id] = nb.g;
                }
                else
                    continue;
            }
            else
            {
                open_list.push(nb);
                open_set.insert(nb.id);
                open_g[nb.id] = nb.g;
            }
            int old_parent = -1;
            if (global_state.count(nb.id))
                old_parent = global_state[nb.id].parent;

            lock_guard<mutex> lock(global_mtx);

            global_state[nb.id] = nb;
            recent_update_buffer.push_back(EdgeUpdate{nb.id, old_parent, nb.parent});
        }

        bool has_work = false;
        {
            if (!open_list.empty() && (open_list.top().g + open_list.top().h) < best_cost)
                has_work = true;
        }
        if (!has_work)
        {
            if (!is_idle)
            {
                idle_count++;
                is_idle = true;
            }
            if (termination())
                done = true;
            this_thread::yield();
            continue;
        }

        if (is_idle)
        {
            idle_count--;
            is_idle = false;
        }

        Node node = open_list.top();

        nodecount++;
        open_list.pop();
        if (open_g.count(node.id) && node.g > open_g[node.id])
            continue;
        if (closed_set.count(node.id))
            continue;
        open_set.erase(node.id);
        open_g.erase(node.id);
        closed_set.insert(node.id);

        if (node.id == goal)
        {
            lock_guard<mutex> lock(best_cost_mtx);
            if (node.g < best_cost)
            {
                best_node = node.id;
                best_cost = node.g;

                int old_parent = -1;
                if (global_state.count(node.id))
                    old_parent = global_state[node.id].parent;

                lock_guard<mutex> g(global_mtx);
                global_state[node.id] = node;
                recent_update_buffer.push_back(EdgeUpdate{nb.id, old_parent, nb.parent});
            }
            continue;
        }

        for (auto &s : get_successor(node))
        {
            int recipient = compute_recipient(s);
            thread_buffers[recipient].send(s);
        }
    }
    if (is_idle)
        idle_count--;
}