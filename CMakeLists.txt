cmake_minimum_required(VERSION 3.12.4)

project(arcdps_bridge VERSION 2.0.0 DESCRIPTION "ArcDPS Extension which sends the events over a named pipe.")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

message(STATUS "Build arcdps_bridge: ${CMAKE_PROJECT_VERSION}")

# General options
option(BRIDGE_BUILD_TESTS "Build test programs" OFF)
option(BRIDGE_ONLY_TEST "Build only for test programs" OFF)

# Print options
message(STATUS "Build Tests: ${BRIDGE_BUILD_TESTS}")

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
set(BRIDGE_COMPILE_DEFINITIONS ${BRIDGE_COMPILE_DEFINITIONS} 
        BRIDGE_VERSION_STR="${CMAKE_PROJECT_VERSION}")

include_directories("${CMAKE_SOURCE_DIR}/third_party/arcdps_unofficial_extras_releases")

# nlohmann_json.
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(third_party/nlohmann_json)

# Libraries to link.
if (${BRIDGE_SET_LOG_LEVEL} GREATER 0)
    set(BRIDGE_LINK_LIBS spdlog::spdlog nlohmann_json::nlohmann_json)
else()
    set(BRIDGE_LINK_LIBS nlohmann_json::nlohmann_json)
endif()

# Compiler flags.
if (MSVC)
    set(BRIDGE_COMPILER_FLAGS /permissive- /W4 /w14640)
else()
    set(BRIDGE_COMPILER_FLAGS -Wall -Wextra -Wpedantic -fvisibility=hidden -Wshadow -Wnon-virtual-dtor -Wold-style-cast 
            -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wdouble-promotion -Wformat=2)
endif ()

# Bridge files.
set(arcdps_bridge_files src/Entry.cpp
    src/ApplicationData.hpp
    src/ApplicationData.cpp
    src/Combat.hpp
    src/Combat.cpp
    src/Extras.hpp
    src/Extras.cpp
    src/Message.hpp
    src/PipeHandler.hpp
    src/PipeHandler.cpp
    src/PipeThread.hpp
    src/PipeThread.cpp
    src/PlayerContainer.hpp
    src/PlayerContainer.cpp
    src/SquadModifyHandler.hpp)

# Library.
if (NOT BRIDGE_ONLY_TEST)
  add_library(arcdps_bridge SHARED ${arcdps_bridge_files} ${LOG_FILES})
  target_compile_definitions(arcdps_bridge PRIVATE ${BRIDGE_COMPILE_DEFINITIONS} BRIDGE_LOG_LEVEL=${BRIDGE_SET_LOG_LEVEL})
  target_link_libraries(arcdps_bridge PRIVATE ${BRIDGE_LINK_LIBS})
  target_compile_options(arcdps_bridge PUBLIC ${BRIDGE_COMPILER_FLAGS})
endif (NOT BRIDGE_ONLY_TEST)

# Tests
if (BRIDGE_BUILD_TESTS)
    # Some test files require functions that are not defined in the header files.
    # Since arcdps_bridge is very small, just build a static version, this is fine and won't add much to compile time.
    add_library(arcdps_bridge_static STATIC ${arcdps_bridge_files}) # No logging when testing.
    target_compile_definitions(arcdps_bridge_static PRIVATE ${BRIDGE_COMPILE_DEFINITIONS} BRIDGE_LOG_LEVEL=0)
    target_link_libraries(arcdps_bridge_static PRIVATE nlohmann_json::nlohmann_json)
    target_compile_options(arcdps_bridge_static PUBLIC ${BRIDGE_COMPILER_FLAGS})

    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/Catch2)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests)
endif (BRIDGE_BUILD_TESTS)

