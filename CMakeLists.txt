cmake_minimum_required(VERSION 3.15)

project(omnifiber
  VERSION 0.1.0
  DESCRIPTION "A fiber library using C++20 coroutines and Boost.Asio"
  LANGUAGES CXX
)

option(OMNIFIBER_INTERNAL "Build omni-fiber as an internal static library" OFF)

# Standard C++20 is required for coroutines
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Find dependencies
# Boost is heavily used (Asio, Log, Format, etc.)
find_package(Boost REQUIRED COMPONENTS log)

# Source files
set(SOURCES
  Fiber.cpp
  SharedAwaitContext.cpp
  SharedAwaitable.cpp
  SingleAwaitContext.cpp
  SingleAwaitable.cpp
  Manager.cpp
)

# Windows-only source for DLL entry point
if(WIN32)
  list(APPEND SOURCES dllmain.cpp)
endif()

# Define the library
add_library(omnifiber STATIC ${SOURCES})

# Include directories
target_include_directories(omnifiber
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

# Compile definitions
target_compile_definitions(omnifiber
  PRIVATE
  OMNIFIBER_EXPORTS
)

# Link dependencies
target_link_libraries(omnifiber
  PUBLIC
  Boost::boost
  PRIVATE
  Boost::log
  Boost::thread
)

# Target properties
set_target_properties(omnifiber PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

# Compiler specific options
if(MSVC)
  target_compile_options(omnifiber PRIVATE /W3 /Zc:preprocessor)
else()
  target_compile_options(omnifiber PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()

# Installation (optional but good practice)
if(NOT OMNIFIBER_INTERNAL)
  include(GNUInstallDirs)
  install(TARGETS omnifiber
    EXPORT omnifiberTargets
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
    Executor.hpp
    Fiber.hpp
    SharedAwaitable.hpp
    FiberFinishNotifier.hpp
    GetCurrentFiber.hpp
    Manager.hpp
    shared.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/omnifiber
  )

  install(EXPORT omnifiberTargets
    FILE omnifiberConfig.cmake
    NAMESPACE omnifiber::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/omnifiber
  )
endif()

add_subdirectory(tests)
add_subdirectory(tools)
