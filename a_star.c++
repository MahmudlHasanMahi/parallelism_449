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
    while (file >> u >> v >> w)
    {
        adj[u].push_back({v, w});
    }
    cout << "graph loaded in memory" << endl;
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

const int CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
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

void sendGraphToClient(crow::websocket::connection &conn)
{
    appendInt(chunkIndex);

    int totalNode = adj.size();
    int totalEdge = 0;
    for (int u = 0; u < totalNode; u++)
    {
        totalEdge += adj[u].size();
    }
    string meta = "{\"type\":\"graph_start\","
                  "\"total_nodes\":" +
                  std::to_string(totalNode) + ","
                                              "\"total_edges\":" +
                  std::to_string(totalEdge) + "}";

    for (int u = 1; u < totalNode; u++)
    {
        bool headerAdded = false;

        int totalChildren = adj[u].size();

        if (!totalChildren)
            continue;

        for (int v = 0; v < totalChildren; v++)
        {

            // does buffer have enough space for
            if ((int)buffer.size() + 8 > CHUNK_SIZE)
            {
                flushChunk(conn);

                if (!headerAdded)
                {
                    
                    appendInt(-1);
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
        break;
    }

    flushChunk(conn);

    // conn.send_text(meta);
}

int main()
{
    string file = "big_graph.txt";
    loadGraph(&file);

    crow::SimpleApp app;

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([](crow::websocket::connection &conn)
                {

            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.insert(&conn);

            

            std::cout << "Client connected\n"; 
              sendGraphToClient(conn); })
        .onclose([](crow::websocket::connection &conn, const std::string &reason, uint16_t code)
                 {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.erase(&conn);
            std::cout << "Client disconnected\n"; })
        .onmessage([](crow::websocket::connection &conn,
                      const std::string &msg, bool is_binary)
                   {
                       std::cout << "Received: " << msg << "\n";
                       conn.send_text("echo: " + msg); });

    std::cout << "Server running on ws://localhost:9001/ws\n";
    app.port(9001).run();

    // th_n = 6;

    // int S = 1, G = 299012;

    // string file = "graph_file/graph_weighted.txt";

    int S = 1, G = 25201;

    thread astarThread(runAStar, S, G);

    astarThread.join();

    return 0;
}