#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
using namespace std;
struct Node
{
    int id;
    int g = 0;
    int h = 0;
    int parent = -1;
};
struct EdgeUpdate
{
    int node_id;
    int old_parent;
    int new_parent;
};
struct thread_buffer
{
    queue<Node> buffer;
    mutex mtx;

    void send(Node n)
    {
        lock_guard<mutex> lock(mtx);
        buffer.push(n);
    }

    bool try_receive(Node &n)
    {
        lock_guard<mutex> lock(mtx);
        if (buffer.empty())
            return false;
        n = buffer.front();
        buffer.pop();
        return true;
    }

    bool empty()
    {
        lock_guard<mutex> lock(mtx);
        return buffer.empty();
    }
};

struct CompareNode
{
    bool operator()(const Node &a, const Node &b) const { return (a.g + a.h) > (b.g + b.h); }
};