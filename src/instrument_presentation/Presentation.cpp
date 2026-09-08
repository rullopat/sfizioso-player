// SPDX-License-Identifier: BSD-2-Clause
#include "Presentation.h"
#include <juce_graphics/juce_graphics.h>
namespace sfizioso_manifest {
std::shared_ptr<const Presentation> buildPresentation(Result metadata,
    const juce::String& selectedPresetPath, const juce::String& artworkPath,
    const AssetReader& readAsset, const juce::String& legacyInstrument,
    const juce::String& legacyPreset)
{
    auto snapshot = std::make_shared<Presentation>();
    snapshot->metadata = std::move(metadata);
    snapshot->selectedPresetPath = selectedPresetPath;
    snapshot->artworkPath = artworkPath;
    snapshot->instrumentName = legacyInstrument;
    snapshot->presetName = legacyPreset;
    if (snapshot->metadata.manifest) snapshot->instrumentName = snapshot->metadata.manifest->name;
    if (const auto* preset = snapshot->preset())
    {
        snapshot->presetName = preset->name;
        if (preset->background.isNotEmpty())
        {
            if (readAsset) snapshot->artwork = readAsset(preset->background);
            snapshot->artworkMime = sfizioso_manifest::imageMime (snapshot->artwork.getData(), snapshot->artwork.getSize());
            if (snapshot->artworkMime.isNotEmpty())
            {
                // Decode only after header limits, then serve a clean static PNG.
                const auto image = juce::ImageFileFormat::loadFrom (snapshot->artwork.getData(), snapshot->artwork.getSize());
                juce::MemoryOutputStream encoded;
                if (image.isValid() && juce::PNGImageFormat().writeImageToStream (image, encoded))
                {
                    snapshot->artwork = encoded.getMemoryBlock();
                    snapshot->artworkMime = "image/png";
                }
                else snapshot->artworkMime.clear();
            }
            if (snapshot->artworkMime.isEmpty())
            {
                snapshot->artwork.reset();
                snapshot->metadata.diagnostics.add ("Artwork missing, unsupported, or exceeds image limits.");
            }
        }
    }
    return snapshot;
}
juce::var presentationToVar(const Presentation& presentation,
    const std::vector<GenericControl>& generic, const std::function<float(int)>& value,
    const juce::String& artworkUrl)
{
    juce::Array<juce::var> out;
    auto add = [&] (int number, const juce::String& label, const juce::String& widget,
                   const juce::String& section, const juce::String& sectionLabel)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("number", number);
        o->setProperty ("label", label.isNotEmpty() ? label : "CC " + juce::String (number));
        o->setProperty ("value", static_cast<double> (value (number)));
        o->setProperty ("widget", widget);
        o->setProperty ("section", section);
        o->setProperty ("sectionLabel", sectionLabel);
        out.add (juce::var (o.get()));
    };
    const auto* preset = presentation.preset();
    if (preset && ! preset->sections.empty())
    {
        for (const auto& section : preset->sections)
            for (const auto& c : section.controls)
            {
                auto label = c.label;
                if (label.isEmpty())
                    for (const auto& g : generic) if (g.number == c.cc) { label = g.label; break; }
                add (c.cc, label, c.widget, section.id, section.label);
            }
    }
    else
        for (const auto& c : generic) add (c.number, c.label, c.isSwitch ? "toggle" : "knob", "", "");
    juce::DynamicObject::Ptr snapshot = new juce::DynamicObject();
    snapshot->setProperty ("controls", out);
    snapshot->setProperty ("instrumentName", presentation.instrumentName);
    snapshot->setProperty ("presetName", presentation.presetName);
    snapshot->setProperty ("accent", preset ? preset->accent : juce::String());
    snapshot->setProperty ("artworkUrl", presentation.artwork.isEmpty() ? juce::String()
        : artworkUrl);
    juce::Array<juce::var> diagnostics;
    for (const auto& diagnostic : presentation.metadata.diagnostics) diagnostics.add (diagnostic);
    snapshot->setProperty ("diagnostics", diagnostics);
    return juce::var (snapshot.get());
}
}
