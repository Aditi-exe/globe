cmake_minimum_required(VERSION 3.24)

# CMake 4.x removed compatibility with CMake < 3.5. Some fetched dependencies
# (glfw/glm/glad) still declare an older minimum; this lets them configure.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(globe
    VERSION 0.0.1
    DESCRIPTION "Real-time satellite visualizer over a 3D Earth"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the dynamic MSVC runtime consistently across ALL targets (including the
# fetched deps) so GLAD/GLFW don't conflict with the app (avoids LNK4098).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# We only ever build the executable, never `cmake --install`. cpr 1.11 emits an
# unconditional install(EXPORT) that references the (non-exported) static libcurl
# target and fails generation; skipping install rules project-wide sidesteps it.
set(CMAKE_SKIP_INSTALL_RULES ON)

include(FetchContent)

# Don't re-run each dependency's git "update" step on reconfigure. Deps pinned to
# a moving ref (stb/sgp4 at master) otherwise try to pull on every reconfigure,
# which can fail to re-apply/update a populated checkout. Once fetched, leave them
# as-is. (Proper fix later: pin stb/sgp4 to specific commits.)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ---------------------------------------------------------------------------
# Dependencies (Phase 0): fetched & built at configure time via FetchContent.
# No system install needed; sources land in the build directory. Requires
# network access on the FIRST configure, and Python 3 (GLAD generates its
# loader). vcpkg remains an option later per PLAN.md if we prefer prebuilt pkgs.
# ---------------------------------------------------------------------------

# GLFW - window, input, and the OpenGL context.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

# GLM - header-only math library (GLSL-like types).
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

# GLAD2 - OpenGL function loader (generated for GL 3.3 core below).
# glad_add_library() is defined in the repo's cmake/ subdir, hence SOURCE_SUBDIR.
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake)

# stb - single-header image loader (the repo has no CMakeLists, so MakeAvailable
# just populates it and sets stb_SOURCE_DIR). Pinned to a specific commit for
# reproducibility (a moving `master` makes FetchContent try to rebase on every
# reconfigure, which fails on a populated checkout).
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 4e9b368bfc071e97206ea5c6adba68dd0cbf8897)

# libsgp4 - SGP4/SDP4 orbit propagation (dnwrnr/sgp4). Its bundled CMake isn't
# consumable as a subproject, so we fetch the sources and compile them ourselves.
# SOURCE_SUBDIR points at the repo's cmake/ dir (no CMakeLists.txt) so
# MakeAvailable populates without add_subdirectory. Pinned to a specific commit
# for reproducibility (see the stb note above).
FetchContent_Declare(sgp4
    GIT_REPOSITORY https://github.com/dnwrnr/sgp4.git
    GIT_TAG b915de601a4af0a9c1c61c54efaafb168ba3defb
    SOURCE_SUBDIR cmake)

# cpr - C++ Requests (HTTP) for fetching live TLEs from CelesTrak. It pulls in
# libcurl, which we build from source and point at the Windows native TLS stack
# (Schannel) so there's NO OpenSSL/zlib/psl system dependency to install.
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPR_INSTALL OFF CACHE BOOL "" FORCE) # we don't install; skip export rules
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# Options for cpr's bundled libcurl: HTTP(S) only, native Schannel TLS, no extras.
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.11.2
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm glad stb sgp4 cpr)

# cpr's bundled libcurl (like GLAD) resets CMP0091, so it ignores
# CMAKE_MSVC_RUNTIME_LIBRARY and compiles against the STATIC CRT (/MTd) -> the
# LNK2038 'MTd vs MDd' / LNK4098 LIBCMTD conflict. Force the dynamic CRT with a
# raw flag on every curl/cpr target that compiles sources.
if(MSVC)
    foreach(_t IN ITEMS libcurl_object libcurl_static libcurl_shared libcurl cpr)
        if(TARGET ${_t})
            get_target_property(_aliased ${_t} ALIASED_TARGET)
            if(NOT _aliased) # can't set options on an alias target
                target_compile_options(${_t} PRIVATE $<IF:$<CONFIG:Debug>,/MDd,/MD>)
            endif()
        endif()
    endforeach()
endif()

file(GLOB SGP4_SOURCES
    "${sgp4_SOURCE_DIR}/libsgp4/*.cc"
    "${sgp4_SOURCE_DIR}/libsgp4/*.cpp")
add_library(sgp4 STATIC ${SGP4_SOURCES})
target_include_directories(sgp4 PUBLIC ${sgp4_SOURCE_DIR}/libsgp4)
if(MSVC)
    set_property(TARGET sgp4 PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Generate the GLAD loader target for OpenGL 3.3 core profile.
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

# GLAD's CMake resets policy CMP0091, so the MSVC_RUNTIME_LIBRARY property is
# ignored for its target (RuntimeLibrary comes out empty -> static CRT -> the
# LNK4098 'LIBCMT conflicts' warning). Force the dynamic CRT with a raw flag.
if(MSVC)
    target_compile_options(glad_gl_core_33 PRIVATE $<IF:$<CONFIG:Debug>,/MDd,/MD>)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(globe
    src/main.cpp
    src/app/Settings.cpp
    src/platform/Window.cpp
    src/platform/Paths.cpp
    src/render/Shader.cpp
    src/render/Texture.cpp
    src/render/Camera.cpp
    src/render/EarthSphere.cpp
    src/render/MarkerBatch.cpp
    src/render/TrackRenderer.cpp
    src/render/stb_image_impl.cpp
    src/render/stb_truetype_impl.cpp
    src/domain/CoordTransforms.cpp
    src/domain/SatelliteCatalog.cpp
    src/domain/Satcat.cpp
    src/domain/Gazetteer.cpp
    src/net/TleSource.cpp
    src/net/SatcatSource.cpp
    src/ui/TextRenderer.cpp
    src/ui/UiContext.cpp
    src/ui/SatelliteList.cpp
    src/ui/LocationPicker.cpp)

target_include_directories(globe PRIVATE src ${stb_SOURCE_DIR} ${sgp4_SOURCE_DIR}/libsgp4)
target_compile_definitions(globe PRIVATE
    GLFW_INCLUDE_NONE
    GLOBE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    GLOBE_VERSION="${PROJECT_VERSION}")
target_link_libraries(globe PRIVATE glfw glm::glm glad_gl_core_33 sgp4 cpr::cpr)

if(MSVC)
    target_compile_options(globe PRIVATE /W4 /permissive-)
else()
    target_compile_options(globe PRIVATE -Wall -Wextra -Wpedantic)
endif()
