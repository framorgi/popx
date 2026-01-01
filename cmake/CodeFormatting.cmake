# Code formatting with clang-format

find_program(CLANG_FORMAT "clang-format")

if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES 
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
    )
    
    add_custom_target(
        format ALL
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format on source files"
        VERBATIM
    )
    
    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking code formatting with clang-format"
        VERBATIM
    )
    
    message(STATUS "clang-format found: ${CLANG_FORMAT}")
    message(STATUS "Auto-formatting is ENABLED - code will be formatted on every build")
    message(STATUS "Run 'cmake --build <build-dir> --target format-check' to check formatting")
else()
    message(WARNING "clang-format not found. Code formatting targets will not be available.")
endif()
