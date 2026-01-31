add_executable(TEST_EXE test.c)
target_link_libraries(TEST_EXE sunflower)
add_test(TEST_1 TEST_EXE)
include_directories(../)