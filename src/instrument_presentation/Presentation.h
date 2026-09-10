// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <instrument_manifest/Manifest.h>
#include <functional>
#include <memory>

namespace sfizioso_manifest {
struct Presentation
{
    Result metadata;
    juce::String selectedPresetPath, instrumentName, presetName, artworkMime, artworkPath;
    juce::MemoryBlock artwork;
    const Preset* preset() const
    {
        if (metadata.manifest)
            for (const auto& p : metadata.manifest->presets)
                if (p.sfz == selectedPresetPath) return &p;
        return nullptr;
    }
};

// Called synchronously at load time; the reader must enforce maxAssetBytes.
using AssetReader = std::function<juce::MemoryBlock(const juce::String&)>;
std::shared_ptr<const Presentation> buildPresentation(Result metadata,
    const juce::String& selectedPresetPath, const juce::String& artworkPath,
    const AssetReader& readAsset, const juce::String& legacyInstrument = {},
    const juce::String& legacyPreset = {});
struct GenericControl { int number; juce::String label; bool isSwitch = false; };
// Values are normalised 0..1. Host supplies its resource URL and engine access.
juce::var presentationToVar(const Presentation&, const std::vector<GenericControl>&,
    const std::function<float(int)>& value, const juce::String& artworkUrl);
}
