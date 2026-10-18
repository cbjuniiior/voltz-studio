cmake_minimum_required(VERSION 3.28...3.30)

# JSON do manifesto do projeto (mesma biblioteca que o frontend do OBS já usa; só nos .cpp do núcleo)
find_package(nlohmann_json 3.11 REQUIRED)

add_library(voltz-core STATIC)
add_library(Voltz::core ALIAS voltz-core)

target_sources(
  voltz-core
  PRIVATE
    annotation-font.cpp
    annotation-journal.cpp
    annotation-model.cpp
    annotation-raster.cpp
    annotation-text.cpp
    atomic-file.cpp
    cursor-track.cpp
    disk-space.cpp
    draw-session.cpp
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
    stroke-store.cpp
    svg-recolor.cpp
    text-layout.cpp
    time-format.cpp
    voltz-version.cpp
  PUBLIC
    annotation-font.hpp
    annotation-journal.hpp
    annotation-model.hpp
    annotation-raster.hpp
    annotation-text.hpp
    atomic-file.hpp
    cursor-track.hpp
    disk-space.hpp
    draw-session.hpp
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
    stroke-store.hpp
    svg-recolor.hpp
    text-layout.hpp
    time-format.hpp
    voltz-version.hpp
)

# Texto das anotações (Plano 4D): as duas DM Sans (Regular e Bold, OFL) da casca embutidas no voltz-core — a sobreposição
# (obs-studio) e a fonte Voltz Anotações (plugin) desenham o texto com os mesmos bytes — e lidas pelo FreeType das deps
# do OBS (o mesmo do text-freetype2; o find_package fica no feature-voltz.cmake, visível para todos os alvos que ligam o
# voltz-core). Ligado PUBLIC: o find_dependencies do OBS aceita o alvo Freetype::Freetype na interface (um PRIVATE
# viraria $<LINK_ONLY:...>, que ele recusa — ver o nlohmann abaixo).
set(_voltz_fonts_dir "${CMAKE_CURRENT_SOURCE_DIR}/../shell/fonts")
set(_voltz_fonts_cpp "${CMAKE_CURRENT_BINARY_DIR}/annotation-font-data.cpp")
add_custom_command(
  OUTPUT "${_voltz_fonts_cpp}"
  COMMAND
    "${CMAKE_COMMAND}" "-DOUTPUT=${_voltz_fonts_cpp}" "-DREGULAR=${_voltz_fonts_dir}/DMSans-Regular.ttf"
    "-DBOLD=${_voltz_fonts_dir}/DMSans-Bold.ttf" -P "${CMAKE_CURRENT_SOURCE_DIR}/embed-fonts.cmake"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/embed-fonts.cmake"
    "${_voltz_fonts_dir}/DMSans-Regular.ttf"
    "${_voltz_fonts_dir}/DMSans-Bold.ttf"
  COMMENT "Voltz: embutindo as fontes do texto das anotacoes"
  VERBATIM
)
target_sources(voltz-core PRIVATE "${_voltz_fonts_cpp}")
target_link_libraries(voltz-core PUBLIC Freetype::Freetype)

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
