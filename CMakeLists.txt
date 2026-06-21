cmake_minimum_required(VERSION 3.22)

project(SfiziosoPlayer VERSION 0.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS deployment target")

set(SFIZIOSO_PLAYER_BASE_BUNDLE_ID "com.sfizioso.player")
set(SFIZIOSO_PLAYER_PLUGIN_BUNDLE_ID "${SFIZIOSO_PLAYER_BASE_BUNDLE_ID}.plugin")
set(SFIZIOSO_PLAYER_AU_BUNDLE_ID "${SFIZIOSO_PLAYER_BASE_BUNDLE_ID}.au")
set(SFIZIOSO_PLAYER_VST3_BUNDLE_ID "${SFIZIOSO_PLAYER_BASE_BUNDLE_ID}.vst3")
set(SFIZIOSO_PLAYER_STANDALONE_BUNDLE_ID "com.sfizioso.player.standalone")

# Shared WebView-UI build helper (npm install -> vite build -> zip -> embed).
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddWebviewUi.cmake)

# ---------------------------------------------------------------------------
# Dependencies — dual-use.
#
# Built STANDALONE (top-level), this repo owns the JUCE + sfizioso engine
# submodules. Consumed via add_subdirectory() by a parent project (e.g. the
# private sample-machine repo), the parent has already added JUCE + sfizioso,
# so we must NOT add them again or CMake errors on duplicate targets.
# PROJECT_IS_TOP_LEVEL distinguishes the two. Either way the libraries below
# resolve `juce::*` and `sfizioso::sfizioso` from whoever added them.
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(external/JUCE)

    set(SFIZZ_JACK       OFF CACHE BOOL "" FORCE)
    set(SFIZZ_RENDER     OFF CACHE BOOL "" FORCE)
    set(SFIZZ_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(SFIZZ_TESTS      OFF CACHE BOOL "" FORCE)
    set(SFIZZ_DEMOS      OFF CACHE BOOL "" FORCE)
    set(SFIZZ_DEVTOOLS   OFF CACHE BOOL "" FORCE)
    set(SFIZZ_SHARED     OFF CACHE BOOL "" FORCE)

    # Demote two warnings newer Apple Clang elevates to errors inside the
    # engine's vendored deps (atomic_queue's template-without-arg-list, Abseil's
    # deprecated __is_trivially_relocatable builtin).
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wno-error=missing-template-arg-list-after-template-kw
            -Wno-deprecated-builtins)
    endif()

    add_subdirectory(external/sfizioso EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# Engine MPE + debug-panel options. Defined here in the foundation repo and
# applied PUBLIC on player_core so every consumer inherits the same value.
# (Moved here from the private consumer as part of the open-core split.)
# ---------------------------------------------------------------------------
option(SAMPLEMACHINE_SFIZZ_HAS_MPE
    "Vendored sfizioso engine exposes the MPE public API (noteOnMPE etc.)" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(_default_debug_midi_panel ON)
else()
    set(_default_debug_midi_panel OFF)
endif()
option(SAMPLEMACHINE_DEBUG_MIDI_PANEL
    "Compile the realtime MIDI debug panel into the player / rompler editors."
    ${_default_debug_midi_panel})

# ---------------------------------------------------------------------------
# core_prefs — global user preference store (theme persistence). BSD-2-Clause.
# Consumed by the Sfizioso Player and by the private branded romplers.
# ---------------------------------------------------------------------------
add_library(core_prefs STATIC src/core_prefs/PreferenceStore.cpp)
target_include_directories(core_prefs PUBLIC src/core_prefs)
target_link_libraries(core_prefs PUBLIC juce::juce_core)

# ---------------------------------------------------------------------------
# player_core — sfizioso wrapper (APVTS params + MIDI dispatch + renderBlock).
# BSD-2-Clause. Shared by the Sfizioso Player app and the private romplers.
# ---------------------------------------------------------------------------
add_library(player_core STATIC src/player_core/PlayerEngine.cpp)
target_include_directories(player_core PUBLIC src/player_core)
target_link_libraries(player_core
    PUBLIC  juce::juce_audio_processors
    PRIVATE sfizioso::sfizioso)

if(SAMPLEMACHINE_SFIZZ_HAS_MPE)
    target_compile_definitions(player_core PUBLIC SAMPLEMACHINE_SFIZZ_HAS_MPE=1)
endif()

if(SAMPLEMACHINE_DEBUG_MIDI_PANEL)
    target_sources(player_core PRIVATE src/player_core/DebugMidiPanel.cpp)
    target_compile_definitions(player_core PUBLIC SAMPLEMACHINE_DEBUG_MIDI_PANEL=1)
    target_link_libraries(player_core PUBLIC juce::juce_gui_basics)
endif()

# Export the shared React UI source dir so consumers' Vite aliases + the
# add_webview_ui() EXTRA_GLOBS can locate it across the submodule boundary.
set(SFIZIOSO_UI_SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-shared/src
    CACHE PATH "Sfizioso shared React UI source directory")

function(sfizioso_set_wrapper_bundle_id source_target wrapper_kind bundle_id)
    if(NOT APPLE)
        return()
    endif()

    set(wrapper_target "${source_target}_${wrapper_kind}")

    if(TARGET ${wrapper_target})
        set_target_properties(${wrapper_target} PROPERTIES
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${bundle_id}")
    endif()

    get_target_property(generated_sources ${source_target} JUCE_GENERATED_SOURCES_DIRECTORY)
    if(NOT generated_sources)
        message(FATAL_ERROR "JUCE_GENERATED_SOURCES_DIRECTORY is missing for ${source_target}")
    endif()

    set(info_plist "${generated_sources}/${wrapper_target}/Info.plist")
    if(NOT EXISTS "${info_plist}")
        message(FATAL_ERROR "Expected generated Info.plist not found: ${info_plist}")
    endif()

    file(READ "${info_plist}" plist_contents)
    string(REGEX REPLACE
        "(<key>CFBundleIdentifier</key>[ \t\r\n]*<string>)[^<]+(</string>)"
        "\\1${bundle_id}\\2"
        patched_plist
        "${plist_contents}")
    file(WRITE "${info_plist}" "${patched_plist}")
endfunction()

# ---------------------------------------------------------------------------
# Sfizioso Player — the standalone / plugin application. AGPLv3.
# Built only when this repo is the top-level project; consumers take just the
# libraries above.
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    juce_add_plugin(SfiziosoPlayer
        VERSION 0.8.0
        COMPANY_NAME "Sfizioso"
        BUNDLE_ID ${SFIZIOSO_PLAYER_PLUGIN_BUNDLE_ID}
        PLUGIN_MANUFACTURER_CODE PtRl
        PLUGIN_CODE Sfzo
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "Sfizioso Player"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE)

    # Give every macOS wrapper its own bundle ID. Logic project compatibility is
    # keyed by the AU type/subtype/manufacturer triple (aumu/Sfzo/PtRl), while
    # duplicate CFBundleIdentifier values can make LaunchServices/window
    # restoration activate the standalone app when the AU is restored.
    sfizioso_set_wrapper_bundle_id(SfiziosoPlayer AU ${SFIZIOSO_PLAYER_AU_BUNDLE_ID})
    sfizioso_set_wrapper_bundle_id(SfiziosoPlayer VST3 ${SFIZIOSO_PLAYER_VST3_BUNDLE_ID})
    sfizioso_set_wrapper_bundle_id(SfiziosoPlayer Standalone
        ${SFIZIOSO_PLAYER_STANDALONE_BUNDLE_ID})

    target_sources(SfiziosoPlayer PRIVATE
        src/player/PlayerProcessor.cpp
        src/player/PlayerEditor.cpp
        src/player/PortableBundle.cpp)

    target_include_directories(SfiziosoPlayer PRIVATE src/player)

    target_compile_definitions(SfiziosoPlayer PUBLIC
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(SfiziosoPlayer
        PRIVATE
            juce::juce_audio_utils
            player_core
            sfizioso::sfizioso
            core_prefs            # SMPL-89 recent-files persistence
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Build the React UI and embed it via juce_add_binary_data.
    add_webview_ui(
        TARGET      Player
        UI_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/src/player/ui
        DATA_TARGET SfiziosoPlayerUiData
        ZIP_NAME    ui_player.zip
        EXTRA_GLOBS ${SFIZIOSO_UI_SHARED}/*.ts
                    ${SFIZIOSO_UI_SHARED}/*.tsx
                    ${SFIZIOSO_UI_SHARED}/*.css)
    target_link_libraries(SfiziosoPlayer PRIVATE SfiziosoPlayerUiData)
    target_compile_definitions(SfiziosoPlayer PUBLIC JUCE_WEB_BROWSER=1)
endif()
