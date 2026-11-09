#include "domain/CoordTransforms.h"

#include <cmath>

namespace globe {

namespace {
constexpr double kEarthRadiusKm = 6378.137; // WGS-84 equatorial radius
constexpr double kPi = 3.14159265358979323846;
} // namespace

glm::vec3 geodeticToGlobe(double latRad, double lonRad, double altKm) {
    double r = 1.0 + altKm / kEarthRadiusKm;
    double phi = kPi / 2.0 - latRad; // colatitude from the north pole
    // EarthSphere mirrors u (uv = 1 - u), so longitude maps as theta = PI - lon.
    double theta = kPi - lonRad;
    double sinPhi = std::sin(phi);
    return glm::vec3(
        static_cast<float>(r * sinPhi * std::cos(theta)),
        static_cast<float>(r * std::cos(phi)),
        static_cast<float>(r * sinPhi * std::sin(theta)));
}

} // namespace globe
