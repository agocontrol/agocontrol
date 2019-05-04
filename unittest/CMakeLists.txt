cmake_minimum_required (VERSION 3.0)

set(TEST_SOURCE_FILES
    runner.cpp
    shared/test_agoconfig.cpp
    shared/test_config_namelist.cpp
    shared/test_agojson.cpp
)

set (TEST_LIBRARIES
    agoclient
    ${CPPUNIT_LIBRARIES}
)

add_executable (testrunner ${TEST_SOURCE_FILES})
target_link_libraries (testrunner ${TEST_LIBRARIES})

add_test( cpp-test testrunner )

add_test(NAME python-test
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-pytest.sh -v ${CMAKE_CURRENT_SOURCE_DIR}/python/
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Shortcut to build and run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS testrunner )

# Shortcut to build and run only cpp tests
add_custom_target(cpp-check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R cpp-test
    DEPENDS testrunner )

# Shortcut to run only python tests
add_custom_target(python-check
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose -R python-test)
