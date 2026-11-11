#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace globe {

namespace {
// Read an environment variable, "" if unset. Uses _dupenv_s on MSVC (getenv
// trips C4996 under /W4); plain getenv elsewhere.
std::string envVar(const char* name) {
#if defined(_WIN32)
    char* buf = nullptr;
    size_t len = 0;
    std::string value;
    if (_dupenv_s(&buf, &len, name) == 0 && buf) {
        value.assign(buf);
    }
    std::free(buf);
    return value;
#else
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : std::string();
#endif
}
} // namespace

std::string userCacheDir(const std::string& appName) {
#if defined(_WIN32)
    const std::string base = envVar("LOCALAPPDATA");
    if (!base.empty()) {
        return (fs::path(base) / appName / "cache").string();
    }
#else
    const std::string xdg = envVar("XDG_CACHE_HOME");
    if (!xdg.empty()) {
        return (fs::path(xdg) / appName).string();
    }
    const std::string home = envVar("HOME");
    if (!home.empty()) {
        return (fs::path(home) / ".cache" / appName).string();
    }
#endif
    // Last resort: working-dir-relative (should rarely happen).
    return (fs::current_path() / "cache").string();
}

std::string userConfigDir(const std::string& appName) {
#if defined(_WIN32)
    const std::string base = envVar("LOCALAPPDATA");
    if (!base.empty()) {
        return (fs::path(base) / appName).string();
    }
#else
    const std::string xdg = envVar("XDG_CONFIG_HOME");
    if (!xdg.empty()) {
        return (fs::path(xdg) / appName).string();
    }
    const std::string home = envVar("HOME");
    if (!home.empty()) {
        return (fs::path(home) / ".config" / appName).string();
    }
#endif
    return fs::current_path().string();
}

} // namespace globe
