#include "platform/Paths.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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

std::string exeDir() {
#if defined(_WIN32)
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) {
            return {};
        }
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2); // path longer than the buffer; grow and retry
    }
    return fs::path(buf).parent_path().string();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return {};
    }
    return fs::weakly_canonical(fs::path(buf)).parent_path().string();
#else
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return p.parent_path().string();
#endif
}

} // namespace globe
