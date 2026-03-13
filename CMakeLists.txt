cmake_minimum_required(VERSION 3.20)
project(yadaac)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-17 clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

add_library(yadaac INTERFACE)

file(GLOB_RECURSE YADAAC_HEADERS "include/*.hpp")

target_sources(yadaac INTERFACE
    ${YADAAC_HEADERS}
)

target_include_directories(yadaac INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(yadaac INTERFACE cxx_std_20)
target_compile_options(yadaac INTERFACE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

option(ENABLE_CLANG_FORMAT "Enable clang-format check target" OFF)
if(ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format-17 clang-format REQUIRED)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking formatting with clang-format"
    )
endif()

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
