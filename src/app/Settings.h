#pragma once

#include <string>

namespace globe {

// Small, persistent user settings, stored as a plain key=value text file in the
// per-user config dir. Deliberately trivial (no SQLite/JSON dependency) — see
// PLAN.md §11. Loading a missing or partial file yields defaults; unknown keys
// are ignored so the format can grow without breaking old files.
struct Settings {
    bool hasObserver = false;
    double obsLat = 0.0;
    double obsLon = 0.0;
    double obsAltKm = 0.0;
    int windowWidth = 1280;
    int windowHeight = 720;

    // Read settings from `path`; returns defaults if the file is absent/unreadable.
    [[nodiscard]] static Settings load(const std::string& path);

    // Write settings to `path`. Returns false if the file couldn't be written.
    bool save(const std::string& path) const;
};

} // namespace globe
