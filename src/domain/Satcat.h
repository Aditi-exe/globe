#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace globe {

// Slow-changing metadata for one cataloged object, from CelesTrak's SATCAT.
struct SatcatInfo {
    std::string objectType; // raw code: "PAY", "R/B", "DEB", "UNK"
    std::string ownerCode;  // raw code: "US", "PRC", "ESA", ...
    std::string launchDate; // "YYYY-MM-DD" (may be empty)
};

// A lookup of SATCAT metadata keyed by NORAD catalog number. Parsed from one or
// more CelesTrak SATCAT CSV files (FORMAT=CSV). Columns are located by their
// header names, so column order/additions don't matter.
class Satcat {
public:
    // Parse a SATCAT CSV file and merge its rows in. Returns rows added. A bad
    // or empty path is a no-op returning 0 (SATCAT is best-effort metadata).
    int loadCsv(const std::string& path);

    // Metadata for a NORAD id, or nullptr if we don't have a row for it.
    [[nodiscard]] const SatcatInfo* find(int norad) const;

    [[nodiscard]] std::size_t size() const { return byNorad_.size(); }

private:
    std::unordered_map<int, SatcatInfo> byNorad_;
};

// Humanize the raw codes for display. Unknown codes are returned as-is so we
// never show a blank; an empty input yields "Unknown".
std::string ownerName(const std::string& code);
std::string objectTypeName(const std::string& code);

} // namespace globe
