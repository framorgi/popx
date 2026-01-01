cmake_minimum_required(VERSION 3.10.0)
project(popx VERSION 1.0)
 

# Don't allow in-source builds
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a separate build directory and run CMake from there.")
endif()


#################################################################
# Include CMake settings
#############################################################

include( cmake/Settings.cmake)
include( cmake/Options.cmake)
include( cmake/Toolchain-x86.cmake) 
include( cmake/ThirdPartyLibs.cmake)
include( cmake/CodeFormatting.cmake)

#Add subdirectories
add_subdirectory(src)
if (UTESTS_ENABLED)
    add_subdirectory(utests)
endif()

 