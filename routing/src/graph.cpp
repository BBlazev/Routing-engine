#include <routing/graph.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <queue>

namespace routing {


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

int CountReachable(const Graph& g, int start)
{
	int size = static_cast<int>(g.adj.size());
	int count = 1;   

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

uint32_t nearest_vertex(const Graph& g, Vec2d p, const std::vector<bool>* allowed)
{
	uint32_t best = 0;
	double bestDist2 = std::numeric_limits<double>::infinity();

	for (uint32_t i = 0; i < g.position.size(); ++i)
	{
		if (allowed && !(*allowed)[i]) continue;

		const double dx = g.position[i].x - p.x;
		const double dy = g.position[i].y - p.y;
		const double d2 = dx * dx + dy * dy;
		if (d2 < bestDist2) { bestDist2 = d2; best = i; }
	}
	return best;
}
std::vector<bool> strongly_connected_from(const Graph& g, uint32_t v)
{
	const size_t n = g.adj.size();

	std::vector<std::vector<uint32_t>> reversed(n);
	for (uint32_t u = 0; u < n; ++u)
		for (const Edge& e : g.adj[u])
			reversed[e.to_node].push_back(u);

	std::vector<bool>    forward(n, false);
	std::vector<bool>    backward(n, false);
	std::queue<uint32_t> q;


	forward[v] = true;
	q.push(v);
	while (!q.empty())
	{
		const uint32_t u = q.front();
		q.pop();
		for (const Edge& e : g.adj[u])
			if (!forward[e.to_node]) { forward[e.to_node] = true; q.push(e.to_node); }
	}

	backward[v] = true;
	q.push(v);
	while (!q.empty())
	{
		const uint32_t u = q.front();
		q.pop();
		for (uint32_t w : reversed[u])
			if (!backward[w]) { backward[w] = true; q.push(w); }
	}

	std::vector<bool> both(n);
	for (size_t i = 0; i < n; ++i)
		both[i] = forward[i] && backward[i];
	return both;
}
} // namespace routing
