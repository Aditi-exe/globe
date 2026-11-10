#include "domain/Satcat.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace globe {

namespace {

// Split one CSV line into fields, honoring double-quoted fields with embedded
// commas and doubled "" escapes. Trailing CR (from CRLF files) is dropped.
std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    ++i; // skip the escaped quote
                } else {
                    inQuotes = false;
                }
            } else {
                cur += ch;
            }
        } else if (ch == '"') {
            inQuotes = true;
        } else if (ch == ',') {
            out.push_back(cur);
            cur.clear();
        } else if (ch != '\r') {
            cur += ch;
        }
    }
    out.push_back(cur);
    return out;
}

// Index of `name` in the header row, or -1.
int columnIndex(const std::vector<std::string>& header, const std::string& name) {
    for (std::size_t i = 0; i < header.size(); ++i) {
        if (header[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Safe fetch of a field by column index ("" if out of range).
std::string field(const std::vector<std::string>& row, int idx) {
    if (idx < 0 || idx >= static_cast<int>(row.size())) {
        return "";
    }
    return row[static_cast<std::size_t>(idx)];
}

} // namespace

int Satcat::loadCsv(const std::string& path) {
    if (path.empty()) {
        return 0;
    }
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[satcat] could not open %s\n", path.c_str());
        return 0;
    }

    std::string line;
    if (!std::getline(file, line)) {
        return 0; // empty file
    }
    const std::vector<std::string> header = splitCsv(line);
    const int iNorad = columnIndex(header, "NORAD_CAT_ID");
    const int iType = columnIndex(header, "OBJECT_TYPE");
    const int iOwner = columnIndex(header, "OWNER");
    const int iLaunch = columnIndex(header, "LAUNCH_DATE");
    if (iNorad < 0) {
        std::fprintf(stderr, "[satcat] %s: no NORAD_CAT_ID column; skipping\n", path.c_str());
        return 0;
    }

    int added = 0;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> row = splitCsv(line);
        const std::string noradStr = field(row, iNorad);
        if (noradStr.empty()) {
            continue;
        }
        int norad = 0;
        try {
            norad = std::stoi(noradStr);
        } catch (const std::exception&) {
            continue; // malformed id
        }
        SatcatInfo info;
        info.objectType = field(row, iType);
        info.ownerCode = field(row, iOwner);
        info.launchDate = field(row, iLaunch);
        byNorad_[norad] = std::move(info);
        ++added;
    }
    std::fprintf(stderr, "[satcat] %s: %d rows\n", path.c_str(), added);
    return added;
}

const SatcatInfo* Satcat::find(int norad) const {
    auto it = byNorad_.find(norad);
    return it == byNorad_.end() ? nullptr : &it->second;
}

std::string objectTypeName(const std::string& code) {
    if (code == "PAY") return "Payload";
    if (code == "R/B") return "Rocket body";
    if (code == "DEB") return "Debris";
    if (code.empty() || code == "UNK") return "Unknown";
    return code;
}

std::string ownerName(const std::string& code) {
    // Common CelesTrak SATCAT owner codes; anything else falls through to the
    // raw code so we never show a blank.
    struct Pair { const char* code; const char* name; };
    static const Pair table[] = {
        {"US", "United States"}, {"CIS", "Russia / CIS"}, {"PRC", "China"},
        {"ESA", "ESA"}, {"EUME", "EUMETSAT"}, {"EUTE", "Eutelsat"},
        {"UK", "United Kingdom"}, {"FR", "France"}, {"GER", "Germany"},
        {"JPN", "Japan"}, {"IND", "India"}, {"CA", "Canada"}, {"AUS", "Australia"},
        {"ITSO", "Intelsat"}, {"SES", "SES"}, {"IRID", "Iridium"},
        {"GLOB", "Globalstar"}, {"ORB", "ORBCOMM"}, {"O3B", "O3b"},
        {"ISS", "ISS (intl.)"}, {"ROC", "Taiwan"}, {"SKOR", "South Korea"},
        {"UKR", "Ukraine"}, {"TURK", "Turkey"}, {"NATO", "NATO"},
        {"IT", "Italy"}, {"SPN", "Spain"}, {"NETH", "Netherlands"},
        {"BRAZ", "Brazil"}, {"ARGN", "Argentina"}, {"ISRA", "Israel"},
        {"LUXE", "Luxembourg"}, {"NOR", "Norway"}, {"SAFR", "South Africa"},
    };
    for (const auto& p : table) {
        if (code == p.code) {
            return p.name;
        }
    }
    if (code.empty() || code == "TBD") {
        return "Unknown";
    }
    return code;
}

} // namespace globe
