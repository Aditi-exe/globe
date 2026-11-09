#pragma once

#include <string>

namespace globe {

// One CelesTrak feed and where its offline fallback lives.
struct TleGroup {
    std::string name;        // short id, also the cache filename (e.g. "stations")
    std::string url;         // CelesTrak GP URL; empty means "never fetch, bundled only"
    std::string bundledPath; // snapshot shipped with the app, used if all else fails
};

// Return a filesystem path to the freshest usable TLE file for this group.
// Policy (matches PLAN.md 3.3, 6h window):
//   1. fresh disk cache (< 6h old)  -> use it, no network
//   2. live fetch from CelesTrak    -> write cache, use it
//   3. stale disk cache             -> use it (better than nothing)
//   4. bundled snapshot             -> last resort
// Never throws: a failed fetch just falls through to the next option.
std::string resolveTleFile(const TleGroup& group, const std::string& cacheDir);

} // namespace globe
