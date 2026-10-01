#include <routing/search.hpp>

#include <algorithm>
#include <queue>

namespace routing {

SearchResult shortest_path(const Graph& g, uint32_t start, uint32_t target, Algorithm algorithm)
{
	const double INF = std::numeric_limits<double>::infinity();
	const uint32_t NONE = std::numeric_limits<uint32_t>::max();
	const size_t n = g.adj.size();

	const double hScale = (algorithm == Algorithm::AStar) ? 0.99 : 0.0;
	const Vec2d goal = g.position[target];

	auto heuristic = [&](uint32_t v) {
		return hScale * std::hypot(g.position[v].x - goal.x, g.position[v].y - goal.y);
	};

	std::vector<double> dist(n, INF);
	std::vector<bool> settled(n, false);
	std::vector<uint32_t> prevVertex(n, NONE);
	std::vector<uint32_t> prevSegment(n, NONE);

	using Item = std::pair<double, uint32_t>;
	std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;

	SearchResult result;
	dist[start] = 0.0;
	pq.emplace(heuristic(start), start);

	while (!pq.empty())
	{
		const uint32_t u = pq.top().second;
		pq.pop();


		if (settled[u]) continue;
		settled[u] = true;

		result.settleOrder.push_back(u);
		if (u == target) break;

		for (const Edge& e : g.adj[u])
		{
			if (settled[e.to_node]) continue;   

			const double candidate = dist[u] + e.length;
			if (candidate < dist[e.to_node])
			{
				dist[e.to_node] = candidate;
				prevVertex[e.to_node] = u;
				prevSegment[e.to_node] = e.segment;
				pq.emplace(candidate + heuristic(e.to_node), e.to_node);
			}
		}
	}

	if (dist[target] == INF) return result;
	result.distance = dist[target];

	for (uint32_t v = target; v != start; v = prevVertex[v])
	{
		result.pathVertices.push_back(v);
		result.pathSegments.push_back(prevSegment[v]);
	}
	result.pathVertices.push_back(start);

	std::reverse(result.pathVertices.begin(), result.pathVertices.end());
	std::reverse(result.pathSegments.begin(), result.pathSegments.end());

	return result;
}

} // namespace routing