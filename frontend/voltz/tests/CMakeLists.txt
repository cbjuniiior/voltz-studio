cmake_minimum_required(VERSION 3.28...3.30)

enable_testing()

add_executable(voltz-core-tests)

target_sources(
  voltz-core-tests
  PRIVATE test-main.cpp test-preview-fit.cpp test-recording-clock.cpp test-time-format.cpp test-version.cpp
)

target_include_directories(voltz-core-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest")
target_link_libraries(voltz-core-tests PRIVATE Voltz::core)
set_target_properties(voltz-core-tests PROPERTIES FOLDER frontend/voltz)

add_test(NAME voltz-core-tests COMMAND voltz-core-tests)
