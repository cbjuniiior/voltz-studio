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
    voltz/project/CursorTracker.cpp
    voltz/project/CursorTracker.hpp
    voltz/project/MonitorLocator.cpp
    voltz/project/MonitorLocator.hpp
    voltz/project/ProjectRecorder.cpp
    voltz/project/ProjectRecorder.hpp
    voltz/shell/AudioPanel.cpp
    voltz/shell/AudioPanel.hpp
    voltz/shell/ErrorBanner.cpp
    voltz/shell/ErrorBanner.hpp
    voltz/shell/ExitDialog.cpp
    voltz/shell/ExitDialog.hpp
    voltz/shell/FrontendBridge.cpp
    voltz/shell/FrontendBridge.hpp
    voltz/shell/Icons.cpp
    voltz/shell/Icons.hpp
    voltz/shell/PresetBuilder.cpp
    voltz/shell/PresetBuilder.hpp
    voltz/shell/PresetsPage.cpp
    voltz/shell/PresetsPage.hpp
    voltz/shell/ProjectsPage.cpp
    voltz/shell/ProjectsPage.hpp
    voltz/shell/PresetThumbnail.cpp
    voltz/shell/PresetThumbnail.hpp
    voltz/shell/RecordingConfig.cpp
    voltz/shell/RecordingConfig.hpp
    voltz/shell/RecordPage.cpp
    voltz/shell/RecordPage.hpp
    voltz/shell/ScenesPanel.cpp
    voltz/shell/ScenesPanel.hpp
    voltz/shell/SharedSources.cpp
    voltz/shell/SharedSources.hpp
    voltz/shell/DeviceSelector.cpp
    voltz/shell/DeviceSelector.hpp
    voltz/shell/SettingsPage.cpp
    voltz/shell/SettingsPage.hpp
    voltz/shell/ShellLocale.cpp
    voltz/shell/ShellLocale.hpp
    voltz/shell/SourcesPanel.cpp
    voltz/shell/SourcesPanel.hpp
    voltz/shell/VoltzPreview.cpp
    voltz/shell/VoltzPreview.hpp
    voltz/shell/VoltzShell.cpp
    voltz/shell/VoltzShell.hpp
    voltz/shell/voltz-shell.qrc
    voltz/shell/WindowChrome.cpp
    voltz/shell/WindowChrome.hpp
)

if(OS_WINDOWS)
  target_link_libraries(obs-studio PRIVATE dwmapi shcore)
endif()
