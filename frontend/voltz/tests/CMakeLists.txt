cmake_minimum_required(VERSION 3.28...3.30)

enable_testing()

add_executable(voltz-core-tests)

target_sources(
  voltz-core-tests
  PRIVATE
    test-main.cpp
    test-annotation-journal.cpp
    test-annotation-model.cpp
    test-annotation-dirty.cpp
    test-annotation-raster.cpp
    test-annotation-text.cpp
    test-atomic-file.cpp
    test-cursor-track.cpp
    test-disk-space.cpp
    test-draw-session.cpp
    test-iso-encoders.cpp
    test-mouse-effects.cpp
    test-mouse-style.cpp
    test-preset-layout.cpp
    test-preview-fit.cpp
    test-project-manifest.cpp
    test-project-markers.cpp
    test-project-naming.cpp
    test-project-timeline.cpp
    test-recording-clock.cpp
    test-recording-error.cpp
    test-recording-files.cpp
    test-recording-settings.cpp
    test-source-tree.cpp
    test-stroke-store.cpp
    test-svg-recolor.cpp
    test-text-layout.cpp
    test-time-format.cpp
    test-version.cpp
)

target_include_directories(voltz-core-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest")
target_link_libraries(voltz-core-tests PRIVATE Voltz::core)
set_target_properties(voltz-core-tests PROPERTIES FOLDER frontend/voltz)

add_test(NAME voltz-core-tests COMMAND voltz-core-tests)
