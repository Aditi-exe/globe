#pragma once

#include <string>

namespace globe {

// Absolute path to a per-user cache directory for this app, created lazily by
// the caller. Windows: %LOCALAPPDATA%\<appName>\cache. POSIX: $XDG_CACHE_HOME/
// <appName> or ~/.cache/<appName>. Falls back to "<cwd>/cache" only if no
// user location can be resolved. Stable regardless of where the exe is launched.
std::string userCacheDir(const std::string& appName);

} // namespace globe
