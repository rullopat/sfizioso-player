#pragma once

#include <juce_core/juce_core.h>

namespace samplemachine
{

// Global user preference store, shared across all Sample Machine
// products on this machine. Reads / writes a single JSON file at
//   ~/Library/Application Support/<vendor-dir>/preferences.json
// (macOS — equivalent userApplicationDataDirectory on Windows / Linux).
// <vendor-dir> defaults to "Sample Machine" and is overridable via the
// SAMPLEMACHINE_PREFS_VENDOR_DIR compile definition (see PreferenceStore.cpp);
// the open standalone Sfizioso Player sets its own so it doesn't share the SM
// product family's prefs file.
//
// Each call loads + parses the file synchronously; the data set is
// expected to be small (a handful of string keys) and writes are user-
// triggered (theme picks etc.), so caching is not worth the staleness
// risk if two plugin instances pick different themes.
//
// Used initially for theme preferences (key per design family, e.g.
// "theme_modern" -> "onyx"). Free namespace rather than a class — there
// is one file, no per-instance state.
namespace PreferenceStore
{
    juce::String get (const juce::String& key, const juce::String& defaultValue);
    void         set (const juce::String& key, const juce::String& value);
}

} // namespace samplemachine
