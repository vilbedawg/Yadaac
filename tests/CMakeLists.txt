find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(run_tests
    DAACConstructionTest.cpp
    StreamTest.cpp
    StreamUtilsTest.cpp
    BaseAllocatorTest.cpp
)

target_link_libraries(run_tests
    PRIVATE
        yadaac
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(run_tests)
