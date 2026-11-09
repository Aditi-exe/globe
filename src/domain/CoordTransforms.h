#pragma once

#include <glm/glm.hpp>

namespace globe {

// Convert geodetic coordinates (latitude/longitude in radians, altitude in km)
// to a position on/above the unit globe where Earth radius == 1.
//
// Convention (must match EarthSphere's UV mapping so markers line up with the
// texture): north pole at +Y, and because the sphere mirrors its texture
// (uv = 1 - u), longitude is mirrored to match as theta = PI - lon.
glm::vec3 geodeticToGlobe(double latRad, double lonRad, double altKm);

} // namespace globe
