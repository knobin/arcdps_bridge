cmake_minimum_required(VERSION 3.12.4)
project(arcdps_bridge VERSION 1.0.1 DESCRIPTION "ArcDPS Extension which sends the events over a named pipe.")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

# Build Type
set(LOG_FILES Log.hpp)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DBRIDGE_DEBUG)
    set(BUILD_TYPE "Debug")
    set(LOG_FILES Log.hpp Log.cpp)
elseif (CMAKE_BUILD_TYPE MATCHES Release)
    set(BUILD_TYPE "Release")
else()
    message(WARNING "No build type specified. Default is Release")
    set(BUILD_TYPE "Release")
endif()
message(STATUS "build type: ${BUILD_TYPE}")

include_directories("third_party/arcdps_unofficial_extras_releases")

# Library
add_library(arcdps_bridge SHARED Entry.cpp
    ApplicationData.hpp
    ApplicationData.cpp
    ${LOG_FILES}
    PipeHandler.hpp
    PipeHandler.cpp
    PipeThread.hpp
    PipeThread.cpp
    PlayerContainer.hpp
    PlayerContainer.cpp)
