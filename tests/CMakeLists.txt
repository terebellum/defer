cmake_minimum_required(VERSION 3.14)
project(defer_tests)

enable_testing()

option(FETCH_CATCH2 "Download and build Catch2" ON)

if(FETCH_CATCH2)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0  # Use the latest stable version
    )
    
    # For Catch2 v3.x, we need to require the C++14 standard
    set(CMAKE_CXX_STANDARD 14)
    
    FetchContent_MakeAvailable(Catch2)
else()
    find_package(Catch2 REQUIRED)
endif()

add_executable(tests
    tests.cpp
)

add_compile_options(-fno-omit-frame-pointer -O0 -pthread)

target_include_directories(tests PRIVATE ../src)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

add_test(NAME tests COMMAND tests)