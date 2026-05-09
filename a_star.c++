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
#include <cstdlib>
#include <crow.h>
#include "ParallelA*/graph.h"
#include <set>
using namespace std;

std::mutex clientsMutex;
std::set<crow::websocket::connection *> clients;
void broadcast(const std::string &message)
{
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto *client : clients)
    {
        client->send_text(message);
    }
}
void loadGraph(string *FILE_NAME)
{

    ios::sync_with_stdio(false);
    cin.tie(NULL);
    ifstream file(*FILE_NAME);
    file.tie(NULL);
    int u, v, w;
    int edge_count = 0;
    unordered_set<int> nodes;

    while (file >> u >> v >> w)
    {
        adj[u].push_back({v, w});
        nodes.insert(u);
        nodes.insert(v);
        edge_count++;
    }
    int last_node = nodes.empty() ? -1 : *max_element(nodes.begin(), nodes.end());
    cout << "Last node (max id): " << last_node << endl;

    cout << "Number of edges: " << edge_count << endl;
    cout << "Number of nodes: " << nodes.size() << endl;
}

void runAStar(int S, int G)
{
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
}

const int CHUNK_SIZE = 1 * 1024 * 1024; // 1M
vector<uint8_t> buffer;
int chunkIndex = 0;
void appendInt(int32_t val)
{
    uint8_t *bytes = reinterpret_cast<uint8_t *>(&val);
    buffer.insert(buffer.end(), bytes, bytes + 4);
}

void flushChunk(crow::websocket::connection &conn)
{
    if (buffer.empty())
        return;
    conn.send_binary(std::string(buffer.begin(), buffer.end()));
    buffer.clear();
    chunkIndex++;
}
void sendMetrics(crow::websocket::connection &conn)
{
    std::string metrics = "{\"type\":\"metrics\","
                          "\"nodes_explored\":" +
                          std::to_string(nodecount) + ","
                                                      "\"best_cost\":" +
                          std::to_string(best_cost) + ","
                                                      "\"global_state_size\":" +
                          std::to_string(global_state.size()) + ","
                                                                "\"buffer_size\":" +
                          std::to_string(recent_update_buffer.size()) + "}";
    conn.send_text(metrics);
}
void sendEdgeUpdates(crow::websocket::connection &conn)
{
    std::cout << "🚀 sendEdgeUpdates started, done=" << done.load() << std::endl;
    int metricTick = 0;

    // ✅ Keep looping until done AND buffer is empty
    while (!done.load() || !recent_update_buffer.empty())
    {
        // Send metrics every 5 ticks

        std::cout << "⏳ buffer size=" << recent_update_buffer.size() << std::endl;

        if (!recent_update_buffer.empty())
        {
            int total = recent_update_buffer.size();
            int sent = 0;

            buffer.clear();
            chunkIndex = 0;
            appendInt(-4);
            appendInt(total);

            for (const auto &edge : recent_update_buffer)
            {
                // recent_update_buffer.erase(recent_update_buffer.begin());

                // cout << recent_update_buffer.size() << endl;
                if ((int)buffer.size() + 12 > CHUNK_SIZE)
                {

                    flushChunk(conn);

                    appendInt(-4);
                    appendInt(total - sent);
                }
                appendInt(edge.node_id);
                appendInt(edge.old_parent);
                appendInt(edge.new_parent);
                sent++;
            }
            flushChunk(conn);
            sendMetrics(conn);
            recent_update_buffer.clear();
            std::cout
                << "Sent " << total << " updates" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ✅ Final batch from global_state after algorithm completes
    std::cout << " Algorithm done, sending final state..." << std::endl;

    int cur = best_node;
    vector<EdgeUpdate> finalPath;
    while (cur != -1 && global_state.count(cur))
    {
        int parent = global_state[cur].parent;
        finalPath.push_back({cur, -1, parent});
        cur = parent;
    }

    if (!finalPath.empty())
    {
        int total = finalPath.size();
        int sent = 0;
        buffer.clear();
        chunkIndex = 0;
        appendInt(-5);
        appendInt(total);
        for (const auto &edge : finalPath)
        {
            if ((int)buffer.size() + 12 > CHUNK_SIZE)
            {
                flushChunk(conn);
                appendInt(-5);
                appendInt(total - sent);
            }
            appendInt(edge.node_id);
            appendInt(edge.old_parent);
            appendInt(edge.new_parent);
            sent++;
        }
        flushChunk(conn);
        std::cout << "Final path sent: " << total << " nodes" << std::endl;
    }
    else
    {
        // ✅ Notify frontend no path found
        buffer.clear();
        chunkIndex = 0;
        appendInt(-6);
        flushChunk(conn);
        std::cout << "No path found!" << std::endl;
    }
}

void sendGraphToClient(crow::websocket::connection &conn, int s, int d)
{
    buffer.clear();
    chunkIndex = 0;

    int totalNode = 0;
    size_t totalEdge = 0;

    for (const auto &[node, neighbors] : adj)
    {
        totalEdge += neighbors.size();
        totalNode++;
    }

    std::string meta = "{\"type\":\"graph_start\","
                       "\"total_nodes\":" +
                       std::to_string(totalNode) + ","
                                                   "\"total_edges\":" +
                       std::to_string(totalEdge) + ","
                                                   "\"source_node\":" +
                       std::to_string(s) + ","
                                           "\"destination_node\":" +
                       std::to_string(d) + ","
                                           "\"threads\":" +
                       std::to_string(th_n) +
                       "}";
    conn.send_text(meta);

    for (const auto &[u, neighbors] : adj)
    {
        int totalChildren = adj[u].size();
        if (totalChildren == 0)
            continue;

        bool headerAdded = false;
        for (int v = 0; v < totalChildren; v++)
        {
            if ((int)buffer.size() + 4 > CHUNK_SIZE)
            {
                if (!headerAdded)
                {
                    appendInt(-1);
                    flushChunk(conn);
                }
                else
                {
                    appendInt(-2);
                    flushChunk(conn);
                }
            }
            if (!headerAdded)
            {
                appendInt(u);
                appendInt(totalChildren);
                headerAdded = true;
            }
            appendInt(adj[u][v].first);
        }
        appendInt(-1);
    }
    flushChunk(conn);
    std::cout << "Graph sent, totalNode=" << totalNode << std::endl;
}

int main()
{
    // int S = 1, G = 15;
    // string FILE_NAME = "graph.txt";
    int S = 1, G = 41189;
    string FILE_NAME = "big_graph.txt";
    string FILE_NAME = "graph_file/graph_weighted.txt";
    loadGraph(&FILE_NAME);
    runAStar(S, G);
    crow::SimpleApp app;

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([S, G](crow::websocket::connection &conn)
                {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.insert(&conn);
            std::cout << "Client connected\n";

            std::thread([&conn, S, G]() {
                std::thread astarThread(runAStar, S, G);
                astarThread.detach();

                sendGraphToClient(conn,S,G);
                std::cout << "Graph sent\n";

                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                sendEdgeUpdates(conn);
            }).detach(); })
        .onclose([](crow::websocket::connection &conn, const std::string &reason, uint16_t code)
                 {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.erase(&conn);
            std::cout << "Client disconnected\n"; })
        .onmessage([](crow::websocket::connection &conn, const std::string &msg, bool is_binary)
                   {
            std::cout << "Received: " << msg << "\n";
            conn.send_text("echo: " + msg); });

    std::cout << "Server running on ws://localhost:9001/ws\n";
    app.port(9001).run();

    return 0;
}