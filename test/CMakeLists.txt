
link_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)

add_executable(jsnpgtest jsnpgtest.c)

target_link_libraries(jsnpgtest jsnpg_static)

add_custom_target(test
         ./run_tests.sh ${CMAKE_CURRENT_BINARY_DIR}/jsnpgtest
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(testutil testutil.c)

target_link_libraries(testutil jsnpg_static)

