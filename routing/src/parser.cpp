#include <routing/parser.hpp>

#include <osmium/io/pbf_input.hpp>
#include <osmium/geom/haversine.hpp>                  
#include <osmium/handler/node_locations_for_ways.hpp> 
#include <osmium/index/map/flex_mem.hpp>              
#include <osmium/visitor.hpp>                         

#include <algorithm>
#include <array>
#include <iostream>
#include <string_view>
#include <unordered_map>

namespace routing {

namespace {

using idx_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<idx_type>;

constexpr std::array<std::string_view, 13> keys =
{ {
	"living_street", "motorway", "motorway_link", "primary", "primary_link",
	"residential", "secondary", "secondary_link", "tertiary", "tertiary_link",
	"trunk", "trunk_link", "unclassified"
} };
static_assert(std::is_sorted(keys.begin(), keys.end()));

bool is_road(std::string_view s)
{
	return std::binary_search(keys.begin(), keys.end(), s);
}

/*
	passA -- WHICH NODES ARE INTERSECTIONS?

In this pass we try to extract only nodes that are meaningful, eg. intersections.
Road can be made of multiple nodes, but not all are intersection, some are there just to make curve on road.
So for each way, we iterate through its nodes and bump up counter depending on how many times it showed up.
Node that has been seen in >= 2 ways is intersection.

We also bump first and last node to be "intersection" because we dont want to exclude ending of any road.

passA result:
nodes -> 199236
intersections -> 29048
only those 29038 we insert in graph (adj list)
*/
struct FileHandler : public osmium::handler::Handler
{
	std::unordered_map<osmium::object_id_type, uint8_t>  count;
	std::unordered_map<osmium::object_id_type, uint32_t> vertex_index;

	void way(const osmium::Way& way)
	{
		const char* highway = way.tags()["highway"];
		if (highway && is_road(highway))
		{
			for (const auto& n : way.nodes())
				count[n.ref()]++;

			count[way.nodes().front().ref()]++;
			count[way.nodes().back().ref()]++;
		}
	}
};

/*
	passB -- WHICH ROADS CONNECT INTERSECTIONS, AND HOW LONG THEY ARE?

We again read all ways.
But this time we itereate over every node and calculate distance from previous node to this one.
When it finds node that is intersection we insert it in adj list and reset dist.
-adj list is whole graph itself-
*/
struct GraphBuilder : public osmium::handler::Handler
{
	const std::unordered_map<osmium::object_id_type, uint32_t>& vertex_index;
	std::vector<std::vector<Edge>> adj;
	std::vector<LatLon> latlon;
	std::vector<Segment> segments;
	std::vector<LatLon> point_latlon;

	explicit GraphBuilder(const std::unordered_map<osmium::object_id_type, uint32_t>& vi)
		: vertex_index(vi)
	{
		adj.resize(vertex_index.size());
		latlon.resize(vertex_index.size());
	}

	void way(const osmium::Way& way)
	{
		const char* highway = way.tags()["highway"];
		if (highway && is_road(highway))
		{
			std::string_view oneway = way.tags()["oneway"] ? way.tags()["oneway"] : "";
			std::string_view junction = way.tags()["junction"] ? way.tags()["junction"] : "";

			const std::string_view hw = highway;
			const bool implied_oneway = hw == "motorway"
				|| junction == "roundabout"
				|| junction == "circular";

			bool forward_ok = true;
			bool backward_ok = true;

			if (oneway == "yes" || oneway == "1" || oneway == "true")
				backward_ok = false;
			else if (oneway == "-1")
				forward_ok = false;
			else if (oneway != "no" && implied_oneway)
				backward_ok = false;

			uint32_t last_vertex = vertex_index.at(way.nodes().front().ref());
			osmium::Location prev_loc = way.nodes().front().location();
			double dist = 0;

			auto seg_start = static_cast<uint32_t>(point_latlon.size());
			bool first = true;

			for (const auto& n : way.nodes())
			{
				dist += osmium::geom::haversine::distance(prev_loc, n.location());
				prev_loc = n.location();

				point_latlon.push_back({ n.location().lat(), n.location().lon() });

				auto y = vertex_index.find(n.ref());

				if (y != vertex_index.end())
				{
					latlon[y->second] = { n.location().lat(), n.location().lon() };

					if (first) { first = false; continue; }

					const auto seg_end = static_cast<uint32_t>(point_latlon.size() - 1);
					const auto seg_id = static_cast<uint32_t>(segments.size());
					segments.push_back({ last_vertex, y->second, seg_start, seg_end - seg_start + 1 });

					if (forward_ok)
						adj[last_vertex].push_back({ y->second, dist, seg_id });
					if (backward_ok)
						adj[y->second].push_back({ last_vertex, dist, seg_id });

					last_vertex = y->second;
					dist = 0;

					seg_start = seg_end;
				}
			}
		}
	}
};

} // namespace

Graph build_graph(const std::string& path)
{
	osmium::io::File file(path);

	osmium::io::Reader reader{ file, osmium::osm_entity_bits::way };
	osmium::io::Reader reader2{ file, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way };

	idx_type index;

	location_handler_type _handler{ index };

	FileHandler passA;

	osmium::apply(reader, passA);

	std::cout << passA.count.size() << std::endl;


	std::vector<osmium::object_id_type> ids;
	for (const auto& [id, c] : passA.count)
		if (c >= 2)
			ids.push_back(id);

	std::sort(ids.begin(), ids.end());

	uint32_t num = 0;
	for (osmium::object_id_type id : ids)
		passA.vertex_index[id] = num++;

	GraphBuilder passB{ passA.vertex_index };
	osmium::apply(reader2, _handler, passB);

	Graph g;
	g.adj = std::move(passB.adj);

	LatLon lo{ +90.0, +180.0 };
	LatLon hi{ -90.0, -180.0 };
	for (const LatLon& p : passB.latlon)
	{
		lo.lat = std::min(lo.lat, p.lat);  hi.lat = std::max(hi.lat, p.lat);
		lo.lon = std::min(lo.lon, p.lon);  hi.lon = std::max(hi.lon, p.lon);
	}
	g.origin = { (lo.lat + hi.lat) / 2.0, (lo.lon + hi.lon) / 2.0 };

	g.position.reserve(passB.latlon.size());
	for (const LatLon& p : passB.latlon)
		g.position.push_back(to_local(p, g.origin));

	g.points.reserve(passB.point_latlon.size());
	for (const LatLon& p : passB.point_latlon)
		g.points.push_back(to_local(p, g.origin));

	g.segments = std::move(passB.segments);

	return g;
}

} // namespace routing