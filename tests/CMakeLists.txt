find_package(GTest REQUIRED)
find_package(Boost REQUIRED COMPONENTS log thread)

include(GoogleTest)

add_executable(omnifiber_test_fiber test_fiber.cpp)
target_link_libraries(omnifiber_test_fiber PRIVATE omnifiber GTest::gtest_main Boost::log Boost::thread)
target_compile_options(omnifiber_test_fiber PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test_fiber PRIVATE -fsanitize=address)
gtest_discover_tests(omnifiber_test_fiber)

add_executable(omnifiber_test_coroutine test_coroutine.cpp)
target_link_libraries(omnifiber_test_coroutine PRIVATE omnifiber GTest::gtest_main Boost::log Boost::thread)
target_compile_options(omnifiber_test_coroutine PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test_coroutine PRIVATE -fsanitize=address)
gtest_discover_tests(omnifiber_test_coroutine)

add_executable(omnifiber_test_get_current_fiber test_get_current_fiber.cpp)
target_link_libraries(omnifiber_test_get_current_fiber PRIVATE omnifiber GTest::gtest_main Boost::log Boost::thread)
target_compile_options(omnifiber_test_get_current_fiber PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test_get_current_fiber PRIVATE -fsanitize=address)
gtest_discover_tests(omnifiber_test_get_current_fiber)

add_executable(omnifiber_test_asio test_asio.cpp)
target_link_libraries(omnifiber_test_asio PRIVATE omnifiber GTest::gtest_main Boost::log Boost::thread)
target_compile_options(omnifiber_test_asio PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test_asio PRIVATE -fsanitize=address)
gtest_discover_tests(omnifiber_test_asio)

add_executable(omnifiber_test_fiber_exceptions test_fiber_exceptions.cpp)
target_link_libraries(omnifiber_test_fiber_exceptions PRIVATE omnifiber GTest::gtest_main Boost::log Boost::thread)
target_compile_options(omnifiber_test_fiber_exceptions PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(omnifiber_test_fiber_exceptions PRIVATE -fsanitize=address)
gtest_discover_tests(omnifiber_test_fiber_exceptions)


