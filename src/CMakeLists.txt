cmake_minimum_required(VERSION 3.10.0)


file(GLOB SOURCES_CPP_FILES "*.cpp")
file(GLOB SOURCES_CONFIG_CPP_FILES "config/*.cpp")

 

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)



# Add executable
add_executable(${PROJECT_NAME}  ${SOURCES_CPP_FILES} )

target_link_libraries( ${PROJECT_NAME} 
                        PUBLIC  sfml-graphics 
                        sfml-window 
                        sfml-system
                        )

              