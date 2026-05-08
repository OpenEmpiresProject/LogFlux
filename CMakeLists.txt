list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")


cmake_minimum_required(VERSION 3.16)

project("LogFlux")

add_subdirectory("LogFlux")


include(ClangFormatUtils)
# Define patterns and ignore file
# Look for all .cpp/.h files in the source tree (GLOB_RECURSE will find files under subdirs)
set(GLOB_PATTERNS "*.cpp" "*.h")
set(IGNORE_FILE "${CMAKE_SOURCE_DIR}/.clang-format-ignore")

# Collect format files
collect_format_files(SOURCE_FILES_TO_FORMAT "${CMAKE_SOURCE_DIR}" "${GLOB_PATTERNS}" "${IGNORE_FILE}")

# Find clang-format
find_program(CLANG_FORMAT_BIN clang-format)

# Check if clang-format was found
if(CLANG_FORMAT_BIN)
    message(STATUS "clang-format found at: ${CLANG_FORMAT_BIN}")
    # Add a custom target to run clang-format
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN}
        -i
        -style=file
        ${SOURCE_FILES_TO_FORMAT}
        COMMENT "Running clang-format on source files"
    )
else()
    message(WARNING "clang-format not found!")
endif()