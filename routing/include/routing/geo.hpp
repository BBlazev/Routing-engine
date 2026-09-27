#pragma once

#include <cmath>
#include <numbers>

namespace routing {

struct Vec2d {
	double x = 0.0;
	double y = 0.0;
};

struct LatLon {
	double lat = 0.0;
	double lon = 0.0;
};


inline Vec2d to_local(LatLon p, LatLon origin) {

	constexpr double kEarthRadius = 6371000.0;                // metres
	constexpr double kDegToRad = std::numbers::pi / 180.0;

	const double x = (p.lon - origin.lon) * kDegToRad * kEarthRadius * std::cos(origin.lat * kDegToRad);
	const double y = (p.lat - origin.lat) * kDegToRad * kEarthRadius;
	return { x, y };
}

} // namespace routing