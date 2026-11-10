#pragma once

#include <string>

namespace globe {

// Return a filesystem path to a usable SATCAT CSV for `group`, or "" if none is
// available. SATCAT is slow-changing metadata, so the window is a week.
// Policy (like resolveTleFile but best-effort — no bundled snapshot):
//   1. fresh disk cache (< 7 days)  -> use it, no network
//   2. live fetch from CelesTrak    -> write cache, use it
//   3. stale disk cache             -> use it
//   4. nothing                      -> "" (panel just omits the metadata)
// Never throws. `group` is the CelesTrak GROUP name (also the cache filename).
std::string resolveSatcatFile(const std::string& group, const std::string& cacheDir);

} // namespace globe
