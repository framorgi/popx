include(FetchContent)

# ---------------------------------------------------------------------------
# SFML — moved here (was in src/CMakeLists.txt) so it is available to
# imgui-sfml which is fetched below.
# ---------------------------------------------------------------------------
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# ---------------------------------------------------------------------------
# Eigen3 — header-only linear algebra
# Populated without running Eigen's CMakeLists to avoid toolchain propagation
# issues in sub-projects.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(Eigen3)
if(NOT eigen3_POPULATED)
    FetchContent_Populate(Eigen3)
endif()
add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
target_include_directories(Eigen3::Eigen INTERFACE ${eigen3_SOURCE_DIR})

# ---------------------------------------------------------------------------
# nlohmann/json — header-only JSON serialisation
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
endif()
add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED GLOBAL)
target_include_directories(nlohmann_json::nlohmann_json INTERFACE
    ${nlohmann_json_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Dear ImGui — fetched manually so we can point imgui-sfml at the sources.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.89.9
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()
# Tell imgui-sfml where to find Dear ImGui sources.
set(IMGUI_DIR "${imgui_SOURCE_DIR}" CACHE PATH "Path to Dear ImGui sources" FORCE)

# ---------------------------------------------------------------------------
# imgui-sfml 2.6 — bridges Dear ImGui and SFML 2.x
# ---------------------------------------------------------------------------
# Do not call find_package(SFML) again; we already have the targets above.
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "Use find_package for SFML inside imgui-sfml" FORCE)
set(IMGUI_SFML_IMGUI_DEMO OFF CACHE BOOL "Build ImGui demo in imgui-sfml" FORCE)

FetchContent_Declare(
    imgui_sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG        v2.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui_sfml)
