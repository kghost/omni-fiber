cmake_minimum_required(VERSION 3.15)

project(omnifiber_tools LANGUAGES CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS log thread)

add_executable(omnifiber_example example.cpp)

target_link_libraries(omnifiber_example
  PRIVATE
    omnifiber
    Boost::log
    Boost::thread
)

target_compile_options(omnifiber_example PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_example PRIVATE -fsanitize=address)
