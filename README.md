# Sfizioso Player

An open SFZ player (VST3 / AU / Standalone) and the shared foundation it is
built on, powered by the [sfizioso](https://github.com/rullopat/sfizioso) SFZ
engine — an independent, MPE-capable fork of sfizz.

## What's here

| Path | What | License |
|------|------|---------|
| `src/player_core/` | sfizioso engine wrapper (APVTS params, MIDI dispatch, render) | BSD-2-Clause |
| `src/core_prefs/`  | global user preference store (theme persistence) | BSD-2-Clause |
| `src/ui-shared/`   | React UI kit + C++/JS bridge (`juceBridge`, `useParam`, knobs, meters, design tokens) | BSD-2-Clause |
| `src/player/`      | the Sfizioso Player application (JUCE + React WebView editor) | AGPL-3.0 |

The shared libraries are permissive so they can also be consumed by other
projects (including closed-source ones); the application itself is AGPLv3.
See [LICENSE](LICENSE).

## Build

```sh
git submodule update --init --recursive          # JUCE + sfizioso (and its nested deps)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Prerequisites (macOS): `brew install node` (Node >= 20) for the WebView UI build.

Artefacts land under `build/SfiziosoPlayer_artefacts/` (VST3 / AU copied to the
user plugin folders; a Standalone app for use without a DAW).

## Consuming the foundation

A parent CMake project can `add_subdirectory()` this repo to reuse the
libraries without building the application: when not the top-level project, the
JUCE / sfizioso submodules and the `SfiziosoPlayer` app target are skipped, and
only `player_core`, `core_prefs`, the `add_webview_ui()` helper, and the
`SFIZIOSO_UI_SHARED` path are exposed. The parent supplies JUCE + sfizioso.
