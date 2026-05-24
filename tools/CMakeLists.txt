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
