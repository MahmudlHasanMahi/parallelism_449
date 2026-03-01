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

struct thread_buffer
{
    queue<Node> buffer; // was: priority_queue<Node, vector<Node>, CompareNode>
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
        n = buffer.front(); // was: buffer.top()
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