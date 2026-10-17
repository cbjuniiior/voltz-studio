cmake_minimum_required(VERSION 3.28...3.30)

# Voltz Studio: fontes próprias (Voltz Mouse e Voltz Anotações). Adicionado por frontend/cmake/feature-voltz.cmake
# depois do voltz-core (que este módulo usa), e não por plugins/CMakeLists.txt.
add_library(voltz-sources MODULE)
add_library(Voltz::sources ALIAS voltz-sources)

target_sources(
  voltz-sources
  PRIVATE
    annotation-feed.cpp
    annotation-feed.hpp
    annotation-source.cpp
    annotation-source.hpp
    mouse-feed.cpp
    mouse-feed.hpp
    mouse-source.cpp
    mouse-source.hpp
    target-size.cpp
    target-size.hpp
    test-vendor.cpp
    test-vendor.hpp
    voltz-sources.cpp
)

target_link_libraries(voltz-sources PRIVATE OBS::libobs OBS::websocket-api Voltz::core)

if(OS_WINDOWS)
  configure_file(cmake/windows/obs-module.rc.in voltz-sources.rc)
  target_sources(voltz-sources PRIVATE voltz-sources.rc)
endif()

set_target_properties_obs(voltz-sources PROPERTIES FOLDER plugins PREFIX "")
