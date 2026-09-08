#include <PlayerProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using samplemachine::PlayerProcessor;
namespace {
void restore (PlayerProcessor& processor, const juce::File& file)
{
    // Exercise the real DAW restore path without writing recent-file preferences.
    processor.getApvts().state.setProperty (samplemachine::PlayerStateProps::sfzPath,
                                          file.getFullPathName(), nullptr);
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
}
struct Temp
{
    juce::File root = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("player-presentation", {}, false);
    Temp() { REQUIRE (root.createDirectory().wasOk()); }
    ~Temp() { root.deleteRecursively(); }
};
}
TEST_CASE ("Player restores authored presentation and discards stale artwork on fallback", "[presentation][processor]")
{
    Temp temp;
    const juce::File example (SFIZIOSO_MANIFEST_EXAMPLE_DIR);
    REQUIRE (example.copyDirectoryTo (temp.root));
    const auto sfz = temp.root.getChildFile ("instrument.sfz");
    PlayerProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    restore (processor, sfz);
    const auto authored = processor.getInstrumentPresentation();
    REQUIRE (authored->preset());
    CHECK (authored->instrumentName == "Evening Signals");
    CHECK (authored->presetName == "Soft Signal");
    CHECK (authored->preset()->sections.size() == 2);
    CHECK (authored->artworkMime == "image/png");
    CHECK_FALSE (authored->artwork.isEmpty());
    CHECK (authored->metadata.diagnostics.isEmpty());
    CHECK (processor.getNumRegions() == 1);
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 0);
    processor.processBlock (audio, midi);
    CHECK (audio.getRMSLevel (0, 0, 256) > 0.0001f);

    REQUIRE (temp.root.getChildFile ("instrument.json").replaceWithText ("{bad"));
    // Retained UI snapshots must stay valid across a host-thread restore.
    std::thread host ([&] { restore (processor, sfz); });
    host.join();
    const auto fallback = processor.getInstrumentPresentation();
    CHECK_FALSE (fallback->preset());
    CHECK (fallback->artwork.isEmpty());
    CHECK (fallback->instrumentName.isEmpty());
    CHECK_FALSE (fallback->metadata.diagnostics.isEmpty());
    CHECK (fallback->artworkPath != authored->artworkPath);
    CHECK_FALSE (authored->artwork.isEmpty());
    CHECK (authored->presetName == "Soft Signal");
    CHECK (processor.getNumRegions() == 1);
    CHECK (processor.getEngine().getCcControls().size() == 4);

    REQUIRE (temp.root.getChildFile ("instrument.json").deleteFile());
    restore (processor, sfz);
    CHECK_FALSE (processor.getInstrumentPresentation()->preset());
    CHECK (processor.getInstrumentPresentation()->metadata.diagnostics.isEmpty());
}

TEST_CASE ("Player ignores retired experimental settings in older session state", "[processor][state]")
{
    PlayerProcessor processor;
    auto state = processor.getApvts().copyState();
    juce::ValueTree retired ("PARAM");
    retired.setProperty ("id", "oversampling", nullptr);
    retired.setProperty ("value", 3.0f, nullptr);
    state.appendChild (retired, nullptr);
    for (auto child : state)
        if (child.getProperty ("id").toString() == "gainDb")
            child.setProperty ("value", -12.0f, nullptr);
    auto xml = state.createXml();
    REQUIRE (xml);
    juce::MemoryBlock data;
    juce::AudioProcessor::copyXmlToBinary (*xml, data);
    processor.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    CHECK (processor.getApvts().getParameter ("oversampling") == nullptr);
    CHECK (processor.getApvts().getRawParameterValue ("gainDb")->load() == -12.0f);
}
