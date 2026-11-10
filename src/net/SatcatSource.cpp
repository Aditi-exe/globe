#include "net/SatcatSource.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <cpr/cpr.h>

namespace fs = std::filesystem;

namespace globe {

namespace {
constexpr auto kMaxAge = std::chrono::hours(24 * 7); // SATCAT changes slowly
constexpr long kTimeoutMs = 8000;

const std::string kBaseUrl = "https://celestrak.org/satcat/records.php?GROUP=";

// Accept only a real SATCAT CSV: it must carry the header field and not be an
// HTML error page. (CelesTrak can answer 200 with an error body.)
bool looksLikeSatcatCsv(const std::string& body) {
    if (body.size() < 40) return false;
    if (body.find('<') != std::string::npos) return false; // HTML error page
    return body.find("NORAD_CAT_ID") != std::string::npos;
}

bool isFresh(const fs::path& p) {
    std::error_code ec;
    auto written = fs::last_write_time(p, ec);
    if (ec) return false;
    return (fs::file_time_type::clock::now() - written) < kMaxAge;
}
} // namespace

std::string resolveSatcatFile(const std::string& group, const std::string& cacheDir) {
    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    const fs::path cache = fs::path(cacheDir) / (group + "-satcat.csv");

    // 1. Fresh cache -> no network.
    if (fs::exists(cache) && isFresh(cache)) {
        std::fprintf(stderr, "[satcat] %s: fresh cache\n", group.c_str());
        return cache.string();
    }

    // 2. Live fetch.
    const std::string url = kBaseUrl + group + "&FORMAT=CSV";
    cpr::Response r = cpr::Get(
        cpr::Url{url},
        cpr::Timeout{kTimeoutMs},
        cpr::Header{{"User-Agent", "globe-satellite-visualizer/0.1"}});

    if (r.status_code == 200 && looksLikeSatcatCsv(r.text)) {
        std::ofstream out(cache, std::ios::binary | std::ios::trunc);
        if (out) {
            out << r.text;
            out.close();
            std::fprintf(stderr, "[satcat] %s: fetched %zu bytes (HTTP %ld, %.0f ms)\n",
                         group.c_str(), r.text.size(), r.status_code, r.elapsed * 1000.0);
            return cache.string();
        }
        std::fprintf(stderr, "[satcat] %s: could not write cache %s\n",
                     group.c_str(), cache.string().c_str());
    } else {
        const std::string reason = !r.error.message.empty()
            ? r.error.message
            : "unexpected response body";
        std::fprintf(stderr, "[satcat] %s: fetch failed — %s (HTTP %ld) — falling back\n",
                     group.c_str(), reason.c_str(), r.status_code);
    }

    // 3. Stale cache beats nothing.
    if (fs::exists(cache)) {
        std::fprintf(stderr, "[satcat] %s: stale cache\n", group.c_str());
        return cache.string();
    }

    // 4. No SATCAT available; metadata is simply omitted.
    std::fprintf(stderr, "[satcat] %s: unavailable (metadata omitted)\n", group.c_str());
    return "";
}

} // namespace globe
