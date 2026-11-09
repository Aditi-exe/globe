#include "domain/SatelliteCatalog.h"

#include "domain/CoordTransforms.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

#include "CoordGeodetic.h"
#include "DateTime.h"
#include "Eci.h"
#include "SGP4.h"
#include "Tle.h"

namespace globe {

namespace {
constexpr double kRadToDeg = 57.29577951308232;

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

int SatelliteCatalog::loadTleFile(const std::string& path, int maxCount) {
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
            st.position = geodeticToGlobe(geo.latitude, geo.longitude, geo.altitude);
            st.valid = true;
        } catch (const std::exception&) {
            st.valid = false; // decayed or out-of-range; skip this frame
        }
    }
}

} // namespace globe
