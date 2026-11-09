#pragma once

#include <glm/glm.hpp>

namespace globe {

// Convert geodetic coordinates (latitude/longitude in radians, altitude in km)
// to a position on/above the unit globe where Earth radius == 1.
//
// Convention (must match EarthSphere's UV mapping so markers line up with the
// texture): north pole at +Y, longitude mapped as theta = lon + PI so that
// texture column u = (lon + 180) / 360.
glm::vec3 geodeticToGlobe(double latRad, double lonRad, double altKm);

} // namespace globe
