cmake_minimum_required(VERSION 3.28...3.30)

# JSON do manifesto do projeto (mesma biblioteca que o frontend do OBS já usa; só nos .cpp do núcleo)
find_package(nlohmann_json 3.11 REQUIRED)

add_library(voltz-core STATIC)
add_library(Voltz::core ALIAS voltz-core)

target_sources(
  voltz-core
  PRIVATE
    atomic-file.cpp
    cursor-track.cpp
    disk-space.cpp
    iso-encoders.cpp
    mouse-effects.cpp
    mouse-style.cpp
    preset-layout.cpp
    preview-fit.cpp
    project-manifest.cpp
    project-markers.cpp
    project-naming.cpp
    project-timeline.cpp
    recording-clock.cpp
    recording-error.cpp
    recording-files.cpp
    recording-settings.cpp
    source-tree.cpp
    svg-recolor.cpp
    time-format.cpp
    voltz-version.cpp
  PUBLIC
    atomic-file.hpp
    cursor-track.hpp
    disk-space.hpp
    iso-encoders.hpp
    live-protocol.hpp
    mouse-effects.hpp
    mouse-style.hpp
    preset-layout.hpp
    preview-fit.hpp
    project-manifest.hpp
    project-markers.hpp
    project-naming.hpp
    project-timeline.hpp
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
# Biblioteca só de cabeçalhos: usa apenas os includes. Ligar o alvo (PRIVATE) vira $<LINK_ONLY:nlohmann_json::...> no
# INTERFACE_LINK_LIBRARIES da lib estática, que o find_dependencies do OBS (obs-studio) recusa.
target_include_directories(
  voltz-core
  SYSTEM
  PRIVATE "$<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>"
)
set_target_properties(voltz-core PROPERTIES FOLDER frontend/voltz)
