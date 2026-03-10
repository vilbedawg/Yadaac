cmake_minimum_required(VERSION 3.20)
project(yadaac)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)

add_library(yadaac INTERFACE)

file(GLOB_RECURSE YADAAC_HEADERS "include/*.hpp")

target_sources(yadaac INTERFACE
    ${YADAAC_HEADERS}
)

target_include_directories(yadaac INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(yadaac INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
