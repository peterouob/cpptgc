cmake_minimum_required(VERSION 4.2)
project(cpptgc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TGC_SANITIZE "Enable ASan + UBSan" ON)
if (TGC_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(cpptgc_table table.cpp)
target_include_directories(cpptgc_table PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(cpptgc_table PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(cpptgc cpptgc.cpp)
target_link_libraries(cpptgc PRIVATE cpptgc_table)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(table_test tests/table_test.cpp)

target_link_libraries(table_test PRIVATE cpptgc_table GTest::gtest_main)
target_compile_options(table_test PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(table_test)