set(all_sources
  benchmark_main.cc
  standalone/benchmark_text_input.cc
  standalone/rcv1_benchmarks.cc
)

if (NOT BUILD_ONLY_STANDALONE_BENCHMARKS)
  set(all_sources ${all_sources}
    input_format_benchmarks.cc
    benchmark_funcs.cc
  )
endif()

add_executable(vw-benchmarks.out
  ${all_sources}
)

find_package(benchmark REQUIRED)

# Add the include directories from vw target for testing
target_include_directories(vw-benchmarks.out PRIVATE $<TARGET_PROPERTY:vw,INCLUDE_DIRECTORIES>)
target_link_libraries(vw-benchmarks.out PRIVATE vw benchmark::benchmark)

# Communicate that Boost Unit Test is being statically linked
if(STATIC_LINK_VW)
  target_compile_definitions(vw-benchmarks.out PRIVATE STATIC_LINK_VW)
endif()

add_test(
  NAME vw_benchmarks
  COMMAND ./vw-benchmarks.out
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
