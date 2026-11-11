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
    double speedKmS = 0.0;    // orbital speed from SGP4 velocity
    int group = 0;            // which feed it came from (for marker coloring)
    glm::vec3 position{0.0f}; // on the unit globe (Earth radius == 1)
    bool valid = false;       // false if propagation failed (e.g. decayed)
};

// A satellite's path over one orbital period, split at "now" into a past and a
// future arc (so they can be colored differently) that share the now point.
// Two versions of each arc: the orbit at true altitude (passes through the
// marker) and the same points dropped to the surface as a ground shadow.
struct SatelliteTrack {
    std::vector<glm::vec3> orbitPast;    // at altitude: one half-orbit ago -> now
    std::vector<glm::vec3> orbitFuture;  // at altitude: now -> one half-orbit ahead
    std::vector<glm::vec3> shadowPast;   // same, dropped to the surface
    std::vector<glm::vec3> shadowFuture;
};

// Where a satellite is in the observer's sky right now.
struct LookAngle {
    double azDeg = 0.0;        // compass bearing, 0 = north, 90 = east
    double elDeg = 0.0;        // angle above the horizon (negative = below)
    double rangeKm = 0.0;      // straight-line distance to the satellite
    double rangeRateKmS = 0.0; // closing (<0) or receding (>0) speed
    bool aboveHorizon = false; // elDeg > 0
    bool valid = false;        // false if propagation failed
};

// One visible pass over the observer. Times are minutes from the moment the
// prediction was computed (so a caller showing a live countdown subtracts the
// elapsed time since then).
struct Pass {
    double aosMinFromNow = 0.0; // acquisition of signal (rises above horizon)
    double losMinFromNow = 0.0; // loss of signal (sets below horizon)
    double maxElDeg = 0.0;      // highest elevation reached during the pass
    double durationMin = 0.0;   // los - aos
    bool inProgress = false;    // true if the satellite is already up now
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
    // maxCount > 0 caps how many are taken from this file (0 = all); used to keep
    // a huge feed like Starlink down to a curated sample. `group` tags every
    // satellite from this file (for per-group marker coloring).
    int loadTleFile(const std::string& path, int maxCount = 0, int group = 0);

    // Propagate every satellite to the current UTC time and refresh states.
    void updateNow();

    // Sample one satellite's path over a full orbital period centered on now,
    // split into past and future arcs, as both the orbit (true altitude) and a
    // ground shadow (surface). `samplesPerArc` is the points per arc. Returns
    // empty arcs on a bad index or if propagation fails.
    [[nodiscard]] SatelliteTrack track(std::size_t index,
                                       int samplesPerArc = 120) const;

    // Where satellite `index` sits in the sky for an observer at the given
    // geodetic location (degrees, km) right now.
    [[nodiscard]] LookAngle lookAngle(std::size_t index, double obsLatDeg,
                                      double obsLonDeg, double obsAltKm) const;

    // The next passes of satellite `index` over that observer, searching forward
    // up to ~48h. A pass is time spent above `horizonDeg`. Returns at most
    // `maxPasses`, soonest first (a pass already in progress is included with
    // aos at/near 0 and inProgress = true).
    [[nodiscard]] std::vector<Pass> predictPasses(std::size_t index,
                                                  double obsLatDeg, double obsLonDeg,
                                                  double obsAltKm, int maxPasses = 3,
                                                  double horizonDeg = 10.0) const;

    [[nodiscard]] const std::vector<SatelliteState>& states() const { return states_; }
    [[nodiscard]] std::size_t size() const { return states_.size(); }

private:
    struct Entry;
    std::vector<std::unique_ptr<Entry>> entries_;
    std::vector<SatelliteState> states_;
};

} // namespace globe
