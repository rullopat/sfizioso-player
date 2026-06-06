# add_webview_ui() — factor the duplicated WebView-UI build pipeline (SMPL-80)
#
# Each WebView target (autosampler, player, rompler) ran an identical block:
# glob ui/src + the shared ui-shared tree, find npm, npm install + vite build,
# zip dist/ flat, embed via juce_add_binary_data. This collapses all of that
# into one call.
#
# The ui-shared dependency globs are passed in via EXTRA_GLOBS rather than
# hardcoded, so the helper holds no path to the shared tree — which is what
# lets the shared UI move to a submodule later (SMPL-79) as a call-site path
# change, not a rewrite of this logic.
#
# Usage:
#   add_webview_ui(
#       TARGET      Autosampler                     # logical name -> Build<TARGET>Ui custom target + comment
#       UI_DIR      ${CMAKE_SOURCE_DIR}/src/autosampler/ui
#       DATA_TARGET SampleMachineUiData             # juce_add_binary_data target name
#       ZIP_NAME    ui_autosampler.zip              # output zip (under the build dir)
#       EXTRA_GLOBS ${CMAKE_SOURCE_DIR}/src/ui-shared/src/*.ts
#                   ${CMAKE_SOURCE_DIR}/src/ui-shared/src/*.tsx
#                   ${CMAKE_SOURCE_DIR}/src/ui-shared/src/*.css)
#
# Behavior is byte-identical to the pre-refactor blocks: same npm flags, same
# flat zip layout (no dist/ prefix), same explicit DEPENDS, same CONFIGURE_DEPENDS
# re-globbing.

function(add_webview_ui)
    cmake_parse_arguments(UI "" "TARGET;UI_DIR;DATA_TARGET;ZIP_NAME" "EXTRA_GLOBS" ${ARGN})

    if(NOT UI_TARGET OR NOT UI_UI_DIR OR NOT UI_DATA_TARGET OR NOT UI_ZIP_NAME)
        message(FATAL_ERROR
            "add_webview_ui: TARGET, UI_DIR, DATA_TARGET and ZIP_NAME are all required.")
    endif()

    set(_ui_dist ${UI_UI_DIR}/dist)
    set(_ui_zip  ${CMAKE_CURRENT_BINARY_DIR}/${UI_ZIP_NAME})

    # Track every input that should re-trigger npm run build. CONFIGURE_DEPENDS
    # re-globs at build time so adding a new component/style file is picked up.
    file(GLOB_RECURSE _ui_inputs CONFIGURE_DEPENDS
        ${UI_UI_DIR}/src/*.ts
        ${UI_UI_DIR}/src/*.tsx
        ${UI_UI_DIR}/src/*.css
        ${UI_EXTRA_GLOBS})

    # Resolve npm once across all calls (cached). Matches the prior
    # "find again if not set" idiom used by the 2nd/3rd copies.
    if(NOT NPM_EXECUTABLE)
        find_program(NPM_EXECUTABLE npm
            HINTS $ENV{HOME}/.nvm/versions/node/*/bin /opt/homebrew/bin /usr/local/bin)
    endif()
    if(NOT NPM_EXECUTABLE)
        message(FATAL_ERROR
            "npm not found. The WebView UI build requires Node/npm "
            "(brew install node, or any nvm-installed Node >= 20).")
    endif()

    add_custom_command(
        OUTPUT ${_ui_zip}
        COMMAND ${NPM_EXECUTABLE} install --no-audit --no-fund --silent
        COMMAND ${NPM_EXECUTABLE} run build --silent
        COMMAND ${CMAKE_COMMAND} -E chdir ${_ui_dist}
                ${CMAKE_COMMAND} -E tar "cf" ${_ui_zip} --format=zip .
        WORKING_DIRECTORY ${UI_UI_DIR}
        DEPENDS ${UI_UI_DIR}/package.json
                ${UI_UI_DIR}/vite.config.ts
                ${UI_UI_DIR}/tsconfig.json
                ${UI_UI_DIR}/index.html
                ${_ui_inputs}
        COMMENT "Building ${UI_TARGET} UI (React + Vite single-file bundle)"
        VERBATIM)
    add_custom_target(Build${UI_TARGET}Ui DEPENDS ${_ui_zip})

    juce_add_binary_data(${UI_DATA_TARGET} SOURCES ${_ui_zip})
    add_dependencies(${UI_DATA_TARGET} Build${UI_TARGET}Ui)
endfunction()
