cmake_minimum_required(VERSION 3.14.0)

file(GLOB SOURCES_CPP_FILES "*.cpp")

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES_CPP_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        sfml-graphics
        sfml-window
        sfml-system
        Eigen3::Eigen
        nlohmann_json::nlohmann_json
)

# std::filesystem needs explicit linking on GCC < 9
target_link_libraries(${PROJECT_NAME} PUBLIC stdc++fs)
