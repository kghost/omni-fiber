cmake_minimum_required(VERSION 3.15)

project(omnifiber_tests LANGUAGES CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS log thread)

add_executable(omnifiber_test test_fiber.cpp)

target_link_libraries(omnifiber_test
  PRIVATE
    omnifiber
    GTest::gtest_main
    Boost::log
    Boost::thread
)

target_compile_options(omnifiber_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test PRIVATE -fsanitize=address)

include(GoogleTest)
gtest_discover_tests(omnifiber_test)
