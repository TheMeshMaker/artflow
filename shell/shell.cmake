# SPDX-FileCopyrightText: 2026 ArtFlow contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# ArtFlow: Pencil2D's core and main window with the ArtFlow shell attached.
# Included by the root CMakeLists.txt after core_lib.cmake and app.cmake,
# whose source lists it reuses. Builds artflow.exe (GUI) and
# artflow-console.exe (same program, with a console for logs).

set(ARTFLOW_STUDIOCORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioLayers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioPenButtons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioPlayback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioTimeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioTimelineModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore/StudioTools.cpp
)

set(ARTFLOW_SHELL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ArtflowIcons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ArtflowModes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ArtflowSelfTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ArtflowShell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ArtflowTheme.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/BeginnerShortcuts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/ModeManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/PenButtonsDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/BrushSlidersPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/FrameStripPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/LayerStackPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/ShellControls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/ShellWidgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/panels/TracksPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell/artflow.qrc
)

# Pencil2D's own entry point is replaced by shell/main.cpp.
set(ARTFLOW_APP_SOURCES ${APP_SOURCES})
list(REMOVE_ITEM ARTFLOW_APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app/src/main.cpp)

# Everything except main() is built once and linked into both programs.
add_library(artflow_objects OBJECT
    ${CORE_LIB_HEADERS}
    ${CORE_LIB_SOURCES}
    ${CORE_LIB_RESOURCES}
    ${APP_HEADERS}
    ${ARTFLOW_APP_SOURCES}
    ${APP_FORMS}
    ${APP_RESOURCES}
    ${ARTFLOW_STUDIOCORE_SOURCES}
    ${ARTFLOW_SHELL_SOURCES}
)

target_include_directories(artflow_objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src
    ${CORE_LIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/studiocore
    ${CMAKE_CURRENT_SOURCE_DIR}/shell
)

set_target_properties(artflow_objects PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/app/ui
)

# The core's QSettings(PENCIL2D, PENCIL2D) calls become ArtFlow's own settings.
target_compile_definitions(artflow_objects PUBLIC PENCIL2D="ArtFlow")

target_link_libraries(artflow_objects PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Xml
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Network
)

target_precompile_headers(artflow_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core_lib/src/corelib-pch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/app-pch.h
)

add_executable(artflow WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/shell/main.cpp)
target_link_libraries(artflow PRIVATE artflow_objects)

if(WIN32)
    add_executable(artflow-console ${CMAKE_CURRENT_SOURCE_DIR}/shell/main.cpp)
    target_link_libraries(artflow-console PRIVATE artflow_objects)
endif()
