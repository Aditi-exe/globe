#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace globe {

// Current computed state of one satellite.
struct SatelliteState {
    std::string name;
    int norad = 0;
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double altKm = 0.0;
    glm::vec3 position{0.0f}; // on the unit globe (Earth radius == 1)
    bool valid = false;       // false if propagation failed (e.g. decayed)
};

// Holds a set of satellites parsed from CelesTrak TLE files and propagates them
// with SGP4. libsgp4 types are kept out of this header (pimpl-style Entry).
class SatelliteCatalog {
public:
    SatelliteCatalog();
    ~SatelliteCatalog();

    SatelliteCatalog(const SatelliteCatalog&) = delete;
    SatelliteCatalog& operator=(const SatelliteCatalog&) = delete;

    // Parse name/line1/line2 triples from a .tle file. Returns satellites added.
    int loadTleFile(const std::string& path);

    // Propagate every satellite to the current UTC time and refresh states.
    void updateNow();

    [[nodiscard]] const std::vector<SatelliteState>& states() const { return states_; }
    [[nodiscard]] std::size_t size() const { return states_.size(); }

private:
    struct Entry;
    std::vector<std::unique_ptr<Entry>> entries_;
    std::vector<SatelliteState> states_;
};

} // namespace globe
