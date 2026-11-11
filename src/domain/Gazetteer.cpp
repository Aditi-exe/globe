#include "domain/Gazetteer.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace globe {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// GeoNames columns we use (0-based): 2 asciiname, 4 lat, 5 lon, 8 country,
// 14 population. The file is tab-separated with 19 columns per line.
constexpr int kColAsciiName = 2;
constexpr int kColLat = 4;
constexpr int kColLon = 5;
constexpr int kColCountry = 8;
constexpr int kColPopulation = 14;
constexpr int kMinCols = 15;

double toDouble(const std::string& s) {
    try {
        return std::stod(s);
    } catch (const std::exception&) {
        return 0.0;
    }
}

long long toLongLong(const std::string& s) {
    try {
        return std::stoll(s);
    } catch (const std::exception&) {
        return 0;
    }
}

} // namespace

int Gazetteer::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[gazetteer] not found: %s (place search disabled)\n",
                     path.c_str());
        return 0;
    }

    std::string line;
    std::vector<std::string> cols;
    cols.reserve(20);
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        // Split on tabs.
        cols.clear();
        std::size_t start = 0;
        while (true) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) {
                cols.push_back(line.substr(start));
                break;
            }
            cols.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (static_cast<int>(cols.size()) < kMinCols) {
            continue;
        }
        Place p;
        p.name = cols[kColAsciiName];
        p.country = cols[kColCountry];
        p.lat = toDouble(cols[kColLat]);
        p.lon = toDouble(cols[kColLon]);
        p.population = toLongLong(cols[kColPopulation]);
        if (p.name.empty()) {
            continue;
        }
        lowerNames_.push_back(toLower(p.name));
        places_.push_back(std::move(p));
    }
    std::fprintf(stderr, "[gazetteer] %zu places from %s\n", places_.size(), path.c_str());
    return static_cast<int>(places_.size());
}

std::vector<const Place*> Gazetteer::search(const std::string& query, int maxResults) const {
    std::vector<const Place*> out;
    const std::string q = toLower(query);
    if (q.empty() || maxResults < 1) {
        return out;
    }

    // Score: 0 exact, 1 prefix, 2 substring; lower is better. Within a tier,
    // larger population wins.
    struct Scored {
        const Place* place;
        int score;
    };
    std::vector<Scored> hits;
    for (std::size_t i = 0; i < places_.size(); ++i) {
        const std::string& name = lowerNames_[i];
        int score;
        if (name == q) {
            score = 0;
        } else if (name.rfind(q, 0) == 0) { // starts with q
            score = 1;
        } else if (name.find(q) != std::string::npos) {
            score = 2;
        } else {
            continue;
        }
        hits.push_back({&places_[i], score});
    }

    std::sort(hits.begin(), hits.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) {
            return a.score < b.score;
        }
        return a.place->population > b.place->population;
    });

    const int n = std::min(static_cast<int>(hits.size()), maxResults);
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out.push_back(hits[static_cast<std::size_t>(i)].place);
    }
    return out;
}

} // namespace globe
