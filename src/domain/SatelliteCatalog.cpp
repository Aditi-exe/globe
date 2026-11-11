#include "domain/SatelliteCatalog.h"

#include "domain/CoordTransforms.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "CoordGeodetic.h"
#include "CoordTopocentric.h"
#include "DateTime.h"
#include "Eci.h"
#include "Observer.h"
#include "SGP4.h"
#include "Tle.h"

namespace globe {

namespace {
constexpr double kRadToDeg = 57.29577951308232;
constexpr double kDegToRad = 0.017453292519943295;

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
} // namespace

struct SatelliteCatalog::Entry {
    libsgp4::Tle tle;
    libsgp4::SGP4 sgp4;
    explicit Entry(const libsgp4::Tle& t) : tle(t), sgp4(t) {}
};

SatelliteCatalog::SatelliteCatalog() = default;
SatelliteCatalog::~SatelliteCatalog() = default;

int SatelliteCatalog::loadTleFile(const std::string& path, int maxCount, int group) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[catalog] could not open %s\n", path.c_str());
        return 0;
    }

    int added = 0;
    std::string name, l1, l2;
    while (std::getline(file, name) && std::getline(file, l1) && std::getline(file, l2)) {
        std::string n = trim(name);
        std::string line1 = trim(l1);
        std::string line2 = trim(l2);
        if (line1.size() < 2 || line1[0] != '1' || line2[0] != '2') {
            continue; // not a well-formed triple; skip
        }
        try {
            libsgp4::Tle tle(n, line1, line2);
            entries_.push_back(std::make_unique<Entry>(tle));
            SatelliteState st;
            st.name = n;
            st.norad = static_cast<int>(tle.NoradNumber());
            st.group = group;
            states_.push_back(std::move(st));
            ++added;
            if (maxCount > 0 && added >= maxCount) {
                break; // curated cap reached (e.g. Starlink sample)
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[catalog] bad TLE '%s': %s\n", n.c_str(), e.what());
        }
    }
    return added;
}

SatelliteTrack SatelliteCatalog::track(std::size_t index, int samplesPerArc) const {
    SatelliteTrack track;
    if (index >= entries_.size() || samplesPerArc < 1) {
        return track;
    }
    // The ground shadow is the sub-satellite point on the surface; lift it a
    // hair so the line doesn't fight the Earth texture for depth. 10 km keeps it
    // reading as a shadow (hugging the surface) without z-fighting.
    constexpr double kShadowLiftKm = 10.0;

    const Entry& e = *entries_[index];
    // Orbital period in minutes from mean motion (revolutions per day).
    const double meanMotion = e.tle.MeanMotion();
    if (meanMotion <= 0.0) {
        return track;
    }
    const double periodMin = 1440.0 / meanMotion;
    const double halfMin = periodMin * 0.5;
    const libsgp4::DateTime now = libsgp4::DateTime::Now(true);

    // Sample at a time offset and append to one arc (orbit + shadow in step).
    auto sample = [&](double offsetMin, std::vector<glm::vec3>& orbit,
                      std::vector<glm::vec3>& shadow) {
        try {
            libsgp4::Eci eci = e.sgp4.FindPosition(now.AddMinutes(offsetMin));
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            orbit.push_back(geodeticToGlobe(geo.latitude, geo.longitude, geo.altitude));
            shadow.push_back(geodeticToGlobe(geo.latitude, geo.longitude, kShadowLiftKm));
        } catch (const std::exception&) {
            // Skip a failed sample; the strip just has a small gap there.
        }
    };

    track.orbitPast.reserve(static_cast<std::size_t>(samplesPerArc) + 1);
    track.orbitFuture.reserve(static_cast<std::size_t>(samplesPerArc) + 1);
    track.shadowPast.reserve(static_cast<std::size_t>(samplesPerArc) + 1);
    track.shadowFuture.reserve(static_cast<std::size_t>(samplesPerArc) + 1);

    // Each arc is sampled from its far end in toward now, so the now point lands
    // at the end of the past arcs and the start of the future arcs (shared join).
    for (int k = samplesPerArc; k >= 0; --k) {
        sample(-halfMin * (static_cast<double>(k) / samplesPerArc),
               track.orbitPast, track.shadowPast);
    }
    for (int k = 0; k <= samplesPerArc; ++k) {
        sample(halfMin * (static_cast<double>(k) / samplesPerArc),
               track.orbitFuture, track.shadowFuture);
    }
    return track;
}

LookAngle SatelliteCatalog::lookAngle(std::size_t index, double obsLatDeg,
                                      double obsLonDeg, double obsAltKm) const {
    LookAngle la;
    if (index >= entries_.size()) {
        return la;
    }
    libsgp4::Observer obs(obsLatDeg, obsLonDeg, obsAltKm);
    const libsgp4::DateTime now = libsgp4::DateTime::Now(true);
    try {
        libsgp4::Eci eci = entries_[index]->sgp4.FindPosition(now);
        libsgp4::CoordTopocentric topo = obs.GetLookAngle(eci);
        la.azDeg = topo.azimuth * kRadToDeg;
        la.elDeg = topo.elevation * kRadToDeg;
        la.rangeKm = topo.range;
        la.rangeRateKmS = topo.rangeRate;
        la.aboveHorizon = topo.elevation > 0.0;
        la.valid = true;
    } catch (const std::exception&) {
        la.valid = false;
    }
    return la;
}

std::vector<Pass> SatelliteCatalog::predictPasses(std::size_t index, double obsLatDeg,
                                                  double obsLonDeg, double obsAltKm,
                                                  int maxPasses, double horizonDeg) const {
    std::vector<Pass> passes;
    if (index >= entries_.size() || maxPasses < 1) {
        return passes;
    }
    libsgp4::Observer obs(obsLatDeg, obsLonDeg, obsAltKm);
    const libsgp4::DateTime start = libsgp4::DateTime::Now(true);
    const double horizonRad = horizonDeg * kDegToRad;
    const double stepMin = 0.5;        // 30 s coarse search step
    const double horizonMin = 48.0 * 60.0;
    const libsgp4::SGP4& sgp4 = entries_[index]->sgp4;

    // Elevation (radians) at `minFromNow`; well below the horizon if propagation
    // fails (treated as "not visible" so a decayed sat just yields no passes).
    auto elevAt = [&](double minFromNow) -> double {
        try {
            libsgp4::Eci eci = sgp4.FindPosition(start.AddMinutes(minFromNow));
            return obs.GetLookAngle(eci).elevation;
        } catch (const std::exception&) {
            return -1.5708; // ~-90 deg
        }
    };

    // Bisect between a (below horizon) and b (above horizon) for the crossing.
    auto refineCross = [&](double a, double b) -> double {
        for (int i = 0; i < 12; ++i) {
            const double m = 0.5 * (a + b);
            if (elevAt(m) > horizonRad) {
                b = m;
            } else {
                a = m;
            }
        }
        return 0.5 * (a + b);
    };

    double prevT = 0.0;
    double prevEl = elevAt(0.0);
    bool above = prevEl > horizonRad;
    double aosT = above ? 0.0 : -1.0;
    double maxEl = above ? prevEl : -1.5708;
    const bool startedAbove = above;

    for (double t = stepMin; t <= horizonMin && static_cast<int>(passes.size()) < maxPasses;
         t += stepMin) {
        const double el = elevAt(t);
        if (!above && el > horizonRad) {
            // Rising edge: AOS between prevT and t.
            aosT = refineCross(prevT, t);
            above = true;
            maxEl = el;
        } else if (above && el <= horizonRad) {
            // Falling edge: LOS between prevT and t.
            const double losT = refineCross(t, prevT); // a=below(t), b=above(prevT)
            Pass p;
            p.aosMinFromNow = aosT < 0.0 ? 0.0 : aosT;
            p.losMinFromNow = losT;
            p.maxElDeg = maxEl * kRadToDeg;
            p.durationMin = losT - p.aosMinFromNow;
            p.inProgress = (aosT <= 0.0) && startedAbove && passes.empty();
            passes.push_back(p);
            above = false;
            maxEl = -1.5708;
        } else if (above) {
            maxEl = std::max(maxEl, el);
        }
        prevT = t;
        prevEl = el;
    }
    return passes;
}

void SatelliteCatalog::updateNow() {
    libsgp4::DateTime now = libsgp4::DateTime::Now(true);
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        SatelliteState& st = states_[i];
        try {
            libsgp4::Eci eci = entries_[i]->sgp4.FindPosition(now);
            libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            st.latDeg = geo.latitude * kRadToDeg;
            st.lonDeg = geo.longitude * kRadToDeg;
            st.altKm = geo.altitude;
            st.speedKmS = eci.Velocity().Magnitude();
            st.position = geodeticToGlobe(geo.latitude, geo.longitude, geo.altitude);
            st.valid = true;
        } catch (const std::exception&) {
            st.valid = false; // decayed or out-of-range; skip this frame
        }
    }
}

} // namespace globe
