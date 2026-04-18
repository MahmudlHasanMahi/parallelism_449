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
    lock_guard<mutex> lock(global_mtx); // Lock added here
    for (auto &e : adj[parent.id])
    {
        int v = e.first, cost = e.second;
        if (cost > 0 && (global_state.find(v) == global_state.end() || parent.g + cost < global_state[v].g))
            neighbors.push_back({v, parent.g + cost, 0, parent.id});
    }
    return neighbors;
}
bool local_termination()
{
    if (idle_count.load() < th_n)
        return false;
    for (auto &tb : thread_buffers)
        if (!tb->empty())
            return false;
    return true;
}

bool global_termination()
{
    int local_idle = local_termination() ? 1 : 0;
    int global_idle = 0;
    // MPI_Allreduce — gathers all local_idle values and ANDs them together.
    // Result is 1 only if every machine is idle simultaneously.
    MPI_Allreduce(&local_idle, &global_idle, 1, MPI_INT,
                  MPI_LAND, MPI_COMM_WORLD);
    return global_idle == 1;
}

std::pair<int, int> compute_recipient(const Node &n)
{
    int total = num_machines * th_n;
    int global = n.id % total;
    int machine = global / th_n;
    int thread = global % th_n;
    return {machine, thread};
}
void broadcast_best_cost(int cost, int node_id)
{
    int payload[2] = {cost, node_id};
    for (int m = 0; m < num_machines; m++)
    {
        if (m == my_machine)
            continue;
        MPI_Send(payload, 2, MPI_INT, m, TAG_BEST_COST, MPI_COMM_WORLD);
    }
}
void broadcast_terminate()
{
    int dummy = 1;
    for (int m = 0; m < num_machines; m++)
    {
        if (m == my_machine)
            continue;
        MPI_Send(&dummy, 1, MPI_INT, m, TAG_TERMINATE, MPI_COMM_WORLD);
    }
}

void send_node(const Node &s)
{
    auto [machine, thread] = compute_recipient(s);
    cout << "send_node: id=" << s.id
         << " → machine=" << machine
         << " thread=" << thread
         << " (num_machines=" << num_machines
         << " th_n=" << th_n << ")" << endl;
    if (machine == my_machine)
    {
        // same container — fast path, no network

        thread_buffers[thread]->send(s);
    }
    else
    {
        // different container — pack and send over MPI
        MPIMessage msg;
        msg.node = s;
        msg.target_thread = thread;
        // MPI_Send blocks until MPI runtime accepts the message.
        // TAG_NODE lets the receiver distinguish this from control messages.
        MPI_Send(&msg, sizeof(MPIMessage), MPI_BYTE,
                 machine, TAG_NODE, MPI_COMM_WORLD);
    }
}

void network_receiver()
{
    cout << "machine " << my_machine << " network_receiver started" << endl;
    while (!done.load())
    {
        MPI_Status status;
        int flag = 0;

        // non-blocking probe — check if any message has arrived
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
                   &flag, &status);

        if (!flag)
        {
            this_thread::yield(); // Prevents CPU burnout
            continue;
        } // nothing yet — spin

        if (status.MPI_TAG == TAG_NODE)
        {
            MPIMessage msg;
            MPI_Recv(&msg, sizeof(MPIMessage), MPI_BYTE,
                     status.MPI_SOURCE, TAG_NODE,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // drop the node into the correct local thread buffer
            // worker threads will pick it up just like a local send
            thread_buffers[msg.target_thread]->send(msg.node);
        }
        else if (status.MPI_TAG == TAG_BEST_COST)
        {
            int payload[2];
            MPI_Recv(payload, 2, MPI_INT,
                     status.MPI_SOURCE, TAG_BEST_COST,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            std::lock_guard<std::mutex> lock(best_cost_mtx);
            if (payload[0] < best_cost)
            {
                best_cost = payload[0];
                best_node = payload[1];
            }
        }
        else if (status.MPI_TAG == TAG_TERMINATE)
        {
            int dummy;
            MPI_Recv(&dummy, 1, MPI_INT,
                     status.MPI_SOURCE, TAG_TERMINATE,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            done = true;
        }
    }
}

void termination_coordinator()
{
    while (!done.load())
    {
        this_thread::sleep_for(chrono::milliseconds(200));

        // Wait for ALL machines to be idle simultaneously
        if (!global_termination())
            continue;

        done = true;
        // No need to broadcast_terminate(); global_termination()
        // guarantees all machines will exit this loop together.
    }
}
void worker(int id, thread_buffer &th_buff, int goal)
{

    priority_queue<Node, vector<Node>, CompareNode> open_list;
    unordered_set<int> open_set, closed_set;
    unordered_map<int, int> open_g;
    bool is_idle = false;
    int i = 0;
    while (!done.load())
    {
        i++;
        // Drain inbox
        Node nb;
        // cout << i << endl;
        while (th_buff.try_receive(nb))
        {
            cout << "machine " << my_machine << " thread " << id
                 << " got node " << nb.id << " g=" << nb.g << endl;
            i++;
            if (closed_set.count(nb.id))
            {
                if (global_state.count(nb.id) && nb.g < global_state[nb.id].g)
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
            lock_guard<mutex> lock(global_mtx);
            global_state[nb.id] = nb;
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
            // if (termination())
            //     done = true;
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
                lock_guard<mutex> g(global_mtx);
                global_state[node.id] = node;
                broadcast_best_cost(best_cost, best_node);
            }
            continue;
        }

        for (auto &s : get_successor(node))
            send_node(s);
    }
    if (is_idle)
        idle_count--;
}

void collect_global_state()
{
    // Each machine sends its local global_state size first,
    // then all its nodes to machine 0
    if (my_machine != 0)
    {
        int sz = (int)global_state.size();
        MPI_Send(&sz, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        for (auto &[id, node] : global_state)
            MPI_Send(&node, sizeof(Node), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }
    else
    {
        // receive from all other machines
        for (int m = 1; m < num_machines; m++)
        {
            int sz;
            MPI_Recv(&sz, 1, MPI_INT, m, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < sz; i++)
            {
                Node n;
                MPI_Recv(&n, sizeof(Node), MPI_BYTE, m, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                global_state[n.id] = n;
            }
        }

        // reconstruct path from best_node back to start
        std::cout << "Path: ";
        int cur = best_node;
        while (cur != -1)
        {
            std::cout << cur << " ";
            cur = global_state.count(cur) ? global_state[cur].parent : -1;
        }
        std::cout << std::endl;
        std::cout << "Cost: " << best_cost << std::endl;
    }
}