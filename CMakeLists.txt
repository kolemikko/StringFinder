cmake_minimum_required(VERSION 3.16)
project(StringFinder)

set(CMAKE_CXX_STANDARD 17)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

find_package(Threads)
include_directories(include)

add_executable(StringFinder main.cpp)
target_link_libraries(StringFinder Threads::Threads)

set(CMAKE_CXX_FLAGS_RELEASE "-O3")