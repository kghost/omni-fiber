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
  SymbolResolver.cpp
)

# Windows-only source for DLL entry point
if(WIN32)
  list(APPEND SOURCES dllmain.cpp)
endif()

# Define the libraries (both standard and ASan)
foreach(suffix "" "-asan")
  set(target "omnifiber${suffix}")
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

  if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_library(DW_LIBRARY NAMES dw REQUIRED)
    target_link_libraries(${target} PRIVATE ${DW_LIBRARY})
  endif()

  # Target properties
  set_target_properties(${target} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
  )

  # Compiler specific options
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3 /Zc:preprocessor)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -fvisibility=hidden)
  endif()

  if(suffix STREQUAL "-asan")
    target_compile_options(${target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address)
  endif()
endforeach()

# Installation (optional but good practice)
if(NOT OMNIFIBER_INTERNAL)
  include(GNUInstallDirs)
  install(TARGETS omnifiber omnifiber-asan
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
