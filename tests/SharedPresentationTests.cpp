// SPDX-License-Identifier: BSD-2-Clause
#include <instrument_presentation/Presentation.h>
#include <catch2/catch_test_macros.hpp>
using namespace sfizioso_manifest;

TEST_CASE ("Shared consumer builds presentation from a caller-owned asset source", "[presentation][shared]")
{
    const juce::File root (SFIZIOSO_MANIFEST_EXAMPLE_DIR);
    auto metadata = loadBeside (root.getChildFile ("instrument.sfz"));
    REQUIRE (metadata.manifest);
    // Simulate a private bundle reader: shared code only sees bounded bytes.
    int reads = 0;
    auto presentation = buildPresentation (metadata, "instrument.sfz", "/art/42",
        [&] (const juce::String& path) { ++reads; return readLocal(root, path, maxAssetBytes); });
    CHECK (reads == 1);
    CHECK (presentation->artworkMime == "image/png");
    CHECK_FALSE (presentation->artwork.isEmpty());
    auto dto = presentationToVar (*presentation, {}, [] (int) { return 0.25f; }, "custom://art/42");
    CHECK (dto["instrumentName"].toString() == "Evening Signals");
    CHECK (dto["artworkUrl"].toString() == "custom://art/42");
    const auto controls = dto["controls"];
    REQUIRE (controls.getArray());
    REQUIRE (controls.size() == 4);
    CHECK (static_cast<double>(controls[0]["value"]) == 0.25);

    auto missing = buildPresentation (metadata, "instrument.sfz", "/art/43", {});
    CHECK (missing->artwork.isEmpty());
    CHECK_FALSE (missing->metadata.diagnostics.isEmpty());
    CHECK_FALSE (presentation->artwork.isEmpty());
}

TEST_CASE ("Shared bridge resolves authored labels and generic fallback", "[presentation][shared]")
{
    Result metadata;
    metadata.manifest.emplace();
    Preset preset; preset.sfz = "sound.sfz";
    Section section; section.id = "tone"; section.label = "Tone";
    section.controls = {{74, {}, "slider"}, {71, {}, "knob"}, {7, "Authored", "knob"}};
    preset.sections.push_back(section); metadata.manifest->presets.push_back(preset);
    auto presentation = buildPresentation(metadata, "sound.sfz", {}, {});
    const std::vector<GenericControl> generic {{74, "Brightness"}, {7, "Volume"}, {64, "Sustain", true}};
    auto dto = presentationToVar(*presentation, generic, [](int) {return 0.0f;}, {});
    auto controls = dto["controls"];
    CHECK (controls[0]["label"].toString() == "Brightness");
    CHECK (controls[1]["label"].toString() == "CC 71");
    CHECK (controls[2]["label"].toString() == "Authored");
    auto fallback = buildPresentation({}, {}, {}, {}, "Legacy", "Preset");
    dto = presentationToVar(*fallback, generic, [](int) {return 1.0f;}, {});
    controls = dto["controls"];
    CHECK (dto["instrumentName"].toString() == "Legacy");
    CHECK (controls[2]["widget"].toString() == "toggle");
    CHECK (dto["artworkUrl"].toString().isEmpty());
}
