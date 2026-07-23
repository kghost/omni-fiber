cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0167 NEW)
project(omni-fiber)

option(OMNIFIBER_INTERNAL "Build omni-fiber as an internal static library" OFF)

# Standard C++20 is required for coroutines
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# Find dependencies
# Boost is heavily used (Asio, Log, Format, etc.)
find_package(Boost REQUIRED COMPONENTS log thread)
find_package(PkgConfig REQUIRED)


# Source files
set(SOURCES
  AwaiterBase.cpp
  Coroutine.cpp
  Fiber.cpp
  Manager.cpp
  SharedAwaitContext.cpp
  SharedAwaiter.cpp
  SymbolResolver.cpp
  SymbolResolverDlAddr.cpp
  SymbolResolverDwfl.cpp
  SymbolResolverWin32.cpp
)

# Define the libraries (both standard and ASan)
foreach(suffix "" "-asan")
  if(suffix STREQUAL "-asan" AND SKIP_ASAN)
    continue()
  endif()
  if(suffix STREQUAL "" AND SKIP_NON_ASAN)
    continue()
  endif()

  set(target "omni-fiber${suffix}")
  add_library(${target} STATIC ${SOURCES})

  # Include directories
  target_include_directories(${target}
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )

  # Compile definitions
  target_compile_definitions(${target}
    PRIVATE
    OMNIFIBER_EXPORTS
  )

  # Link dependencies
  target_link_libraries(${target}
    PUBLIC
    Boost::boost
    PRIVATE
    Boost::log
    Boost::thread
  )

  if(WIN32)
    target_link_libraries(${target} PRIVATE dbghelp)
  endif()

  if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang") AND NOT ANDROID)
    target_link_libraries(${target} PUBLIC stdc++exp)
  endif()

  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ANDROID AND NOT WIN32)
    pkg_check_modules(LIBDW REQUIRED libdw)
    target_link_libraries(${target} PRIVATE ${LIBDW_LIBRARIES})
  endif()

  # Target properties
  set_target_properties(${target} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    SOVERSION 0
  )

  # Compiler specific options
  if(WIN32)
    target_compile_options(${target} PRIVATE /W3 /Zc:preprocessor /Od)
  else()
    target_compile_options(${target} PRIVATE -Wall -fvisibility=hidden)
  endif()

  if(suffix STREQUAL "-asan")
    target_compile_options(${target} PRIVATE -fsanitize=address)
    target_link_options(${target} PRIVATE -fsanitize=address)
  endif()
endforeach()

# Installation (optional but good practice)
if(NOT OMNIFIBER_INTERNAL)
  include(GNUInstallDirs)
  install(TARGETS omni-fiber omni-fiber-asan
    EXPORT OmniFiberTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(FILES
    Asio.hpp
    Coroutine.hpp
    Event.hpp
    EventQueue.hpp
    ExternalQueue.hpp
    Executor.hpp
    Fiber.hpp
    Mutex.hpp
    SharedAwaiter.hpp
    FiberFinishNotifier.hpp
    GetCurrentOmniFiber.hpp
    Manager.hpp
    Pipe.hpp
    RemoteCall.hpp
    Select.hpp
    SelectPair.hpp
    SelectPairList.hpp
    SelectPairDynamicList.hpp
    OmniYield.hpp
    shared.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/omni-fiber
  )

  install(EXPORT OmniFiberTargets
    FILE OmniFiberConfig.cmake
    NAMESPACE OmniFiber::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/omni-fiber
  )
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
add_subdirectory(tools)
