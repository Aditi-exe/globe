#include "net/TleSource.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <cpr/cpr.h>

namespace fs = std::filesystem;

namespace globe {

namespace {
constexpr auto kMaxAge = std::chrono::hours(6); // re-download window (PLAN.md 3.3)
constexpr long kTimeoutMs = 8000;               // don't hang startup on a dead link

// CelesTrak occasionally answers 200 with an error/HTML body (bad group, rate
// limited). Only accept text that actually contains TLE line-1 and line-2 rows.
bool looksLikeTle(const std::string& body) {
    if (body.size() < 130) return false;             // one triple is ~140+ chars
    if (body.find('<') != std::string::npos) return false; // HTML error page
    return body.find("\n1 ") != std::string::npos
        && body.find("\n2 ") != std::string::npos;
}

bool isFresh(const fs::path& p) {
    std::error_code ec;
    auto written = fs::last_write_time(p, ec);
    if (ec) return false;
    return (fs::file_time_type::clock::now() - written) < kMaxAge;
}
} // namespace

std::string resolveTleFile(const TleGroup& group, const std::string& cacheDir) {
    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    const fs::path cache = fs::path(cacheDir) / (group.name + ".tle");

    // 1. Fresh cache -> no network needed; app opens instantly.
    if (fs::exists(cache) && isFresh(cache)) {
        std::fprintf(stderr, "[tle] %s: fresh cache\n", group.name.c_str());
        return cache.string();
    }

    // 2. Live fetch from CelesTrak (TLS via Schannel; cpr returns status 0 on
    //    connection failure rather than throwing).
    if (!group.url.empty()) {
        cpr::Response r = cpr::Get(
            cpr::Url{group.url},
            cpr::Timeout{kTimeoutMs},
            cpr::Header{{"User-Agent", "globe-satellite-visualizer/0.1"}});

        if (r.status_code == 200 && looksLikeTle(r.text)) {
            std::ofstream out(cache, std::ios::binary | std::ios::trunc);
            if (out) {
                out << r.text;
                out.close();
                std::fprintf(stderr, "[tle] %s: fetched %zu bytes (HTTP %ld, %.0f ms)\n",
                             group.name.c_str(), r.text.size(),
                             r.status_code, r.elapsed * 1000.0);
                return cache.string();
            }
            std::fprintf(stderr, "[tle] %s: could not write cache %s\n",
                         group.name.c_str(), cache.string().c_str());
        }
        // Why it failed: prefer curl's transport error (DNS/TLS/timeout), else
        // note the response didn't look like TLE despite the status code.
        const std::string reason = !r.error.message.empty()
            ? r.error.message
            : "unexpected response body";
        std::fprintf(stderr, "[tle] %s: fetch failed — %s (HTTP %ld) — falling back\n",
                     group.name.c_str(), reason.c_str(), r.status_code);
    }

    // 3. Stale cache beats a stale snapshot.
    if (fs::exists(cache)) {
        std::fprintf(stderr, "[tle] %s: stale cache\n", group.name.c_str());
        return cache.string();
    }

    // 4. Bundled snapshot.
    std::fprintf(stderr, "[tle] %s: bundled snapshot\n", group.name.c_str());
    return group.bundledPath;
}

} // namespace globe
