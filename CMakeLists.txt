cmake_minimum_required(VERSION 3.12.4)

project(arcdps_bridge VERSION 1.0.4 DESCRIPTION "ArcDPS Extension which sends the events over a named pipe.")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

message(STATUS "Build arcdps_bridge: ${CMAKE_PROJECT_VERSION}")

# Default log levels.
set(BRIDGE_DEFAULT_LOG_LEVEL_RELEASE 3)
set(BRIDGE_DEFAULT_LOG_LEVEL_DEBUG 4)

# Log level.
if (BRIDGE_LOG_LEVEL MATCHES "^[0-9]+$")
    set(BRIDGE_SET_LOG_LEVEL ${BRIDGE_LOG_LEVEL})
endif()

# Build Type
set(LOG_FILES src/Log.hpp)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if (NOT BRIDGE_SET_LOG_LEVEL)
        message(STATUS "No logging level defined, default for debug is ${BRIDGE_DEFAULT_LOG_LEVEL_DEBUG}.")
        set(BRIDGE_SET_LOG_LEVEL ${BRIDGE_DEFAULT_LOG_LEVEL_DEBUG})
    endif()
    set(BRIDGE_COMPILE_DEFINITIONS BRIDGE_BUILD_DEBUG)
    set(BUILD_TYPE "Debug")
else()
    if (NOT CMAKE_BUILD_TYPE MATCHES Release)
        message(WARNING "No build type specified. Default is Release")
    endif()
    if (NOT BRIDGE_SET_LOG_LEVEL)
        message(STATUS "No logging level defined, default for release is ${BRIDGE_DEFAULT_LOG_LEVEL_RELEASE}.")
        set(BRIDGE_SET_LOG_LEVEL ${BRIDGE_DEFAULT_LOG_LEVEL_RELEASE})
    endif()
    set(BUILD_TYPE "Release")
endif()
message(STATUS "build type: ${BUILD_TYPE}")

# Log level files.
if (${BRIDGE_SET_LOG_LEVEL} GREATER 0)
    set(LOG_FILES src/Log.hpp src/Log.cpp)
    add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/spdlog")
endif()
message(STATUS "Logging level set to: ${BRIDGE_SET_LOG_LEVEL}.")
set(BRIDGE_COMPILE_DEFINITIONS ${BRIDGE_COMPILE_DEFINITIONS} BRIDGE_LOG_LEVEL=${BRIDGE_SET_LOG_LEVEL})

include_directories("${CMAKE_SOURCE_DIR}/third_party/arcdps_unofficial_extras_releases")

# Library
add_library(arcdps_bridge SHARED src/Entry.cpp
    src/ApplicationData.hpp
    src/ApplicationData.cpp
    ${LOG_FILES}
    src/PipeHandler.hpp
    src/PipeHandler.cpp
    src/PipeThread.hpp
    src/PipeThread.cpp
    src/PlayerContainer.hpp
    src/PlayerContainer.cpp)

target_compile_definitions(arcdps_bridge PRIVATE ${BRIDGE_COMPILE_DEFINITIONS} BRIDGE_VERSION_STR="${CMAKE_PROJECT_VERSION}")

if (${BRIDGE_SET_LOG_LEVEL} GREATER 0)
    target_link_libraries(arcdps_bridge PRIVATE spdlog::spdlog)
endif()

if (MSVC)
    target_compile_options(arcdps_bridge PUBLIC /permissive- /W4 /w14640)
else()
    target_compile_options(arcdps_bridge PUBLIC
            -Wall -Wextra -Wpedantic -fvisibility=hidden -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
            -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wdouble-promotion -Wformat=2)
endif ()
