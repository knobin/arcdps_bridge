cmake_minimum_required(VERSION 3.12.4)
project(arcdps_bridge VERSION 1 DESCRIPTION "ArcDPS Extension which sends the events over a named pipe.")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

# Build Type
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
    set(BUILD_TYPE "Debug")
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    set(BUILD_TYPE "Release")
else()
    message(WARNING "No build type specified. Default is Release")
    set(BUILD_TYPE "Release")
endif()
message(STATUS "build type: ${BUILD_TYPE}")

include_directories("third_party/arcdps_unofficial_extras_releases")

# Library
add_library(arcdps_bridge SHARED bridge.cpp)
