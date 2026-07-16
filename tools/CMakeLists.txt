foreach(suffix "" "-asan")
  if(suffix STREQUAL "-asan" AND SKIP_ASAN)
    continue()
  endif()
  if(suffix STREQUAL "" AND SKIP_NON_ASAN)
    continue()
  endif()

  find_package(Boost REQUIRED COMPONENTS log thread)

  add_executable(omnifiber_example${suffix} example.cpp)

  target_link_libraries(omnifiber_example${suffix}
    PRIVATE
      omni-fiber${suffix}
      Boost::log
      Boost::thread
  )

  if(suffix STREQUAL "-asan")
    target_compile_options(omnifiber_example${suffix} PRIVATE -fsanitize=address)
    target_link_options(omnifiber_example${suffix} PRIVATE -fsanitize=address)
  endif()
endforeach()
