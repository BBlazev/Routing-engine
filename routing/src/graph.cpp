#include <routing/graph.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <queue>

namespace routing {

int CountReachable(const Graph& g, int start)
{
    int size  = static_cast<int>(g.adj.size());
    int count = 0;

    std::vector<bool>    visited(static_cast<size_t>(size), false);
    std::queue<uint32_t> que;

    que.push(static_cast<uint32_t>(start));
    visited[static_cast<size_t>(start)] = true;

    while (!que.empty())
    {
        uint32_t node = que.front();
        que.pop();

        for (const Edge& neighbor : g.adj[node])
        {
            if (!visited[neighbor.to_node])
            {
                visited[neighbor.to_node] = true;
                que.push(neighbor.to_node);
                count++;
            }
        }
    }
    return count;
}

std::vector<double> dijkstra(const Graph& g, uint32_t start)
{
    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> dist(g.adj.size(), INF);
    dist[start] = 0.0;

    using que_item = std::pair<double, uint32_t>;
    std::priority_queue<que_item, std::vector<que_item>, std::greater<que_item>> pq;

    pq.emplace(0.0, start);

    while (!pq.empty())
    {
        double   dist_to_current = pq.top().first;
        uint32_t current         = pq.top().second;

        pq.pop();

        if (dist_to_current > dist[current]) continue;

        for (const Edge& edge : g.adj[current])
        {
            assert(edge.to_node < g.adj.size());

            double candidate = dist_to_current + edge.length;
            if (candidate < dist[edge.to_node])
            {
                dist[edge.to_node] = candidate;
                pq.emplace(candidate, edge.to_node);
            }
        }
    }

    return dist;
}

void print_stats(const Graph& g)
{
    std::cout << "vertices: " << g.adj.size() << std::endl;

    std::map<size_t, int> hist;
    for (const auto& list : g.adj)
        hist[list.size()]++;

    for (auto& [degree, cnt] : hist)
        std::cout << "deg " << degree << ": " << cnt << "\n";
}

} // namespace routing
