# Voltz Studio — código próprio ligado ao alvo obs-studio

option(ENABLE_VOLTZ_TESTS "Build Voltz Studio unit tests" ON)

add_subdirectory(voltz/core)

if(ENABLE_VOLTZ_TESTS)
  add_subdirectory(voltz/tests)
endif()

target_link_libraries(obs-studio PRIVATE Voltz::core)
