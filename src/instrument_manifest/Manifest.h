#pragma once
// SPDX-License-Identifier: BSD-2-Clause
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace sfizioso_manifest
{
inline constexpr std::size_t maxJsonBytes = 1024 * 1024;
inline constexpr std::size_t maxAssetBytes = 8 * 1024 * 1024;
struct Control { int cc = 0; juce::String label, widget { "knob" }; };
struct Section { juce::String id, label; std::vector<Control> controls; };
struct Preset
{
    juce::String id, name, category, author, sfz, background, accent;
    std::vector<Section> sections;
};
struct Manifest
{
    juce::String id, name, author, version;
    std::vector<Preset> presets;
};
struct Result
{
    std::optional<Manifest> manifest;
    juce::StringArray diagnostics;
};
// All functions are load-time only. Never call from the audio callback.
Result parse (const void* bytes, std::size_t size);
std::pair<juce::String, juce::String> parseLegacy (const void* bytes, std::size_t size);
bool isPackagePath (const juce::String& path);
// Canonicalises every component, including symlinks, before reading bounded data.
juce::MemoryBlock readLocal (const juce::File& root, const juce::String& path,
                             std::size_t limit);
Result loadBeside (const juce::File& sfz);
// Checks type and dimensions before a browser/image decoder sees the bytes.
// Returns image/png or image/jpeg, or empty on rejection.
juce::String imageMime (const void* bytes, std::size_t size);
}
