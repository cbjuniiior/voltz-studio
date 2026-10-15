# Voltz Studio — código próprio ligado ao alvo obs-studio

option(ENABLE_VOLTZ_TESTS "Build Voltz Studio unit tests" ON)

set(VOLTZ_VERSION_MAJOR 1)
set(VOLTZ_VERSION_MINOR 0)
set(VOLTZ_VERSION_PATCH 0)
set(VOLTZ_VERSION "${VOLTZ_VERSION_MAJOR}.${VOLTZ_VERSION_MINOR}.${VOLTZ_VERSION_PATCH}")

add_subdirectory(voltz/core)

if(ENABLE_VOLTZ_TESTS)
  add_subdirectory(voltz/tests)
endif()

target_link_libraries(obs-studio PRIVATE Voltz::core)
target_compile_definitions(
  obs-studio
  PRIVATE VOLTZ_STUDIO "VOLTZ_VERSION=\"${VOLTZ_VERSION}\"" "VOLTZ_CONFIG_DIR=\"Voltz Studio\""
)

target_sources(
  obs-studio
  PRIVATE
    voltz/shell/FrontendBridge.cpp
    voltz/shell/FrontendBridge.hpp
    voltz/shell/VoltzPreview.cpp
    voltz/shell/VoltzPreview.hpp
)
