#pragma once

#include <string>

namespace globe {

// Absolute path to a per-user cache directory for this app, created lazily by
// the caller. Windows: %LOCALAPPDATA%\<appName>\cache. POSIX: $XDG_CACHE_HOME/
// <appName> or ~/.cache/<appName>. Falls back to "<cwd>/cache" only if no
// user location can be resolved. Stable regardless of where the exe is launched.
std::string userCacheDir(const std::string& appName);

// Absolute path to a per-user config directory for this app (for settings that
// should persist, unlike the disposable cache). Windows: %LOCALAPPDATA%\<appName>.
// POSIX: $XDG_CONFIG_HOME/<appName> or ~/.config/<appName>. Falls back to the
// current directory only if no user location can be resolved.
std::string userConfigDir(const std::string& appName);

// Absolute path to the directory containing the running executable (not the
// working directory). Used to locate bundled assets in a packaged build, where
// the app is launched by double-click from an arbitrary folder. Returns "" if
// the path can't be determined.
std::string exeDir();

} // namespace globe
