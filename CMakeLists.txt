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

include(FetchContent)

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

FetchContent_MakeAvailable(glfw glm glad)

# Generate the GLAD loader target for OpenGL 3.3 core profile.
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(globe
    src/main.cpp
    src/platform/Window.cpp)

target_include_directories(globe PRIVATE src)
target_compile_definitions(globe PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(globe PRIVATE glfw glm::glm glad_gl_core_33)

if(MSVC)
    target_compile_options(globe PRIVATE /W4 /permissive-)
else()
    target_compile_options(globe PRIVATE -Wall -Wextra -Wpedantic)
endif()
