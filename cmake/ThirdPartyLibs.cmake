include(FetchContent)

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
