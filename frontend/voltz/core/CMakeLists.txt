cmake_minimum_required(VERSION 3.28...3.30)

add_library(voltz-core STATIC)
add_library(Voltz::core ALIAS voltz-core)

target_sources(
  voltz-core
  PRIVATE
    preset-layout.cpp
    preview-fit.cpp
    recording-clock.cpp
    recording-error.cpp
    recording-files.cpp
    recording-settings.cpp
    source-tree.cpp
    svg-recolor.cpp
    time-format.cpp
    voltz-version.cpp
  PUBLIC
    preset-layout.hpp
    preview-fit.hpp
    recording-clock.hpp
    recording-error.hpp
    recording-files.hpp
    recording-settings.hpp
    source-tree.hpp
    svg-recolor.hpp
    time-format.hpp
    voltz-version.hpp
)

# Inclui a partir de frontend/ para usar <voltz/core/...>
target_include_directories(voltz-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_features(voltz-core PUBLIC cxx_std_17)
set_target_properties(voltz-core PROPERTIES FOLDER frontend/voltz)
