cmake_minimum_required(VERSION 3.20)
project(yadaac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(yadaac INTERFACE)
add_library(yadaac::yadaac ALIAS yadaac)

target_include_directories(yadaac INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(yadaac INTERFACE cxx_std_20)
target_compile_options(yadaac INTERFACE -Wall -Wextra -Werror -pedantic)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS yadaac
    EXPORT yadaacTargets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT yadaacTargets
    FILE yadaacTargets.cmake
    NAMESPACE yadaac::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yadaac
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/yadaacConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/yadaacConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/yadaacConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yadaac
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/yadaacConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/yadaacConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yadaac
)

enable_testing()
add_subdirectory(tests)
