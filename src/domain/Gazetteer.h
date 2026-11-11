#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace globe {

// One populated place from the GeoNames gazetteer.
struct Place {
    std::string name;         // ASCII name (GeoNames `asciiname`)
    std::string country;      // ISO 3166-1 alpha-2 code (e.g. "US", "FR")
    double lat = 0.0;
    double lon = 0.0;
    long long population = 0;
};

// Loads a GeoNames "cities" dump (tab-separated, e.g. cities15000.txt) and
// answers name searches. The file is bundled, not fetched; if it's missing the
// gazetteer is simply empty (ready() == false) and the UI falls back to manual
// lat/lon entry. See assets/data/README for how to obtain the file.
class Gazetteer {
public:
    // Parse a GeoNames tab-separated file. Returns the number of places loaded.
    int load(const std::string& path);

    [[nodiscard]] bool ready() const { return !places_.empty(); }
    [[nodiscard]] std::size_t size() const { return places_.size(); }

    // Best matches for `query` (case-insensitive): exact names first, then
    // prefix matches, then substring matches, each tier ordered by population.
    // Returns up to `maxResults` places. An empty query returns nothing.
    [[nodiscard]] std::vector<const Place*> search(const std::string& query,
                                                   int maxResults) const;

private:
    std::vector<Place> places_;
    std::vector<std::string> lowerNames_; // places_[i].name lowercased, for search
};

} // namespace globe
