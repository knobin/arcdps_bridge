
set(BRIDGE_SRC "${CMAKE_SOURCE_DIR}/src")

# Function for define tests.
function(define_test)
    cmake_parse_arguments(
        TEST_PREFIX
        ""
        "NAME"
        "ARGS;FILES;LINKS"
        ${ARGN}
    )

    if (TEST_PREFIX_NAME)
        if (TEST_PREFIX_FILES)
            add_executable(${TEST_PREFIX_NAME} ${TEST_PREFIX_FILES})
            target_compile_definitions(${TEST_PREFIX_NAME} PRIVATE ${BRIDGE_COMPILE_DEFINITIONS} BRIDGE_LOG_LEVEL=0)
            target_include_directories(${TEST_PREFIX_NAME} PRIVATE ${BRIDGE_SRC})
            if (TEST_PREFIX_LINKS)
                target_link_libraries(${TEST_PREFIX_NAME} PRIVATE ${TEST_PREFIX_LINKS} Catch2::Catch2WithMain)
            else (TEST_PREFIX_LINKS)
                target_link_libraries(${TEST_PREFIX_NAME} PRIVATE Catch2::Catch2WithMain)
            endif (TEST_PREFIX_LINKS)
            add_test(NAME ${TEST_PREFIX_NAME} COMMAND ${TEST_PREFIX_NAME} ${TEST_PREFIX_ARGS})
        else (TEST_PREFIX_FILES)
            message(SEND_ERROR "No files specified for ${TEST_PREFIX_NAME}")
        endif (TEST_PREFIX_FILES)
    else (TEST_PREFIX_NAME)
        message(SEND_ERROR "No name specified for test!")
    endif (TEST_PREFIX_NAME)
endfunction()

# Add tests here!
define_test(NAME CombatTest FILES ${BRIDGE_SRC} FuzzHelper.hpp CombatTest.cpp LINKS arcdps_bridge_static nlohmann_json::nlohmann_json)
define_test(NAME ExtrasTest FILES ${BRIDGE_SRC} FuzzHelper.hpp ExtrasTest.cpp LINKS arcdps_bridge_static nlohmann_json::nlohmann_json)
define_test(NAME MessageTest FILES ${BRIDGE_SRC} FuzzHelper.hpp MessageTest.cpp LINKS arcdps_bridge_static nlohmann_json::nlohmann_json)
define_test(NAME SquadTest FILES ${BRIDGE_SRC} FuzzHelper.hpp SquadTest.cpp LINKS arcdps_bridge_static nlohmann_json::nlohmann_json)