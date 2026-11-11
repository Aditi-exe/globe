#include "app/Settings.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace globe {

namespace {

std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return "";
    }
    const std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

int toInt(const std::string& s, int fallback) {
    try {
        return std::stoi(s);
    } catch (const std::exception&) {
        return fallback;
    }
}

double toDouble(const std::string& s, double fallback) {
    try {
        return std::stod(s);
    } catch (const std::exception&) {
        return fallback;
    }
}

} // namespace

Settings Settings::load(const std::string& path) {
    Settings s;
    std::ifstream file(path);
    if (!file) {
        return s; // defaults
    }
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        if (key == "observer") {
            s.hasObserver = toInt(val, 0) != 0;
        } else if (key == "obs_lat") {
            s.obsLat = toDouble(val, s.obsLat);
        } else if (key == "obs_lon") {
            s.obsLon = toDouble(val, s.obsLon);
        } else if (key == "obs_alt_km") {
            s.obsAltKm = toDouble(val, s.obsAltKm);
        } else if (key == "window_w") {
            s.windowWidth = toInt(val, s.windowWidth);
        } else if (key == "window_h") {
            s.windowHeight = toInt(val, s.windowHeight);
        }
        // Unknown keys are ignored.
    }
    return s;
}

bool Settings::save(const std::string& path) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec); // ensure config dir exists
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        std::fprintf(stderr, "[settings] could not write %s\n", path.c_str());
        return false;
    }
    file << "# globe settings\n";
    file << "observer=" << (hasObserver ? 1 : 0) << "\n";
    file << "obs_lat=" << obsLat << "\n";
    file << "obs_lon=" << obsLon << "\n";
    file << "obs_alt_km=" << obsAltKm << "\n";
    file << "window_w=" << windowWidth << "\n";
    file << "window_h=" << windowHeight << "\n";
    return true;
}

} // namespace globe
