#include <PlayerEngine.h>

#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace samplemachine;

namespace
{

void prepare (PlayerEngine& engine, MpeMode mode, const char* sfz)
{
    engine.prepareToPlay (44100.0, 256);
    engine.setMpeMode (mode, 48);
    REQUIRE (engine.loadSfzString (sfz, "test://channel-routing.sfz"));
}

void process (PlayerEngine& engine, juce::MidiBuffer midi, int blocks = 1)
{
    juce::AudioBuffer<float> audio (2, 256);
    for (int block = 0; block < blocks; ++block)
    {
        audio.clear();
        juce::MidiBuffer events;
        if (block == 0)
            events.swapWith (midi);
        engine.processBlock (audio, events);
    }
}

constexpr const char* channelOne = R"(
    <region> lochan=1 hichan=1 sample=*sine ampeg_release=0.001
)";

constexpr const char* channelTwo = R"(
    <region> lochan=2 hichan=2 sample=*sine ampeg_release=0.001
)";

constexpr const char* independentCc32 = R"(
    <region> lochan=1 hichan=1 locc32=1 hicc32=1 sample=*sine ampeg_release=0.001
    <region> lochan=2 hichan=2 locc32=2 hicc32=2 sample=*saw ampeg_release=0.001
)";

} // namespace

TEST_CASE ("PlayerEngine MPE None preserves channel-1 region routing", "[channel_routing][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::None, channelOne);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 0);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 0);

    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 0);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine MPE None preserves channel-2 region routing", "[channel_routing][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::None, channelTwo);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 0);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 0);

    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 0);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine keeps fixed-channel CC32 articulation independent", "[channel_routing][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::None, independentCc32);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 32, 1), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 32, 2), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 1);
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 1);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 2);
}

TEST_CASE ("PlayerEngine source Note Off does not release another channel", "[channel_routing][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::None, R"(
        <region> lochan=1 hichan=2 sample=*sine ampeg_release=0.001
    )");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 0);
    process (engine, std::move (midi));
    REQUIRE (engine.getNumActiveVoices() == 2);

    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    process (engine, std::move (midi), 32);
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine MPE Full still honors lochan and hichan", "[channel_routing][mpe][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::Full, channelTwo);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 1);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine MPE Full does not bypass Member CC32 filtering", "[channel_routing][mpe][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::Full, R"(
        <region> lochan=2 hichan=2 locc32=2 hicc32=2 sample=*sine
    )");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (2, 32, 2), 0);
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, juce::uint8 (100)), 1);
    process (engine, std::move (midi));
    CHECK (engine.getNumActiveVoices() == 0);
}

TEST_CASE ("PlayerEngine routes scoped Program Change while preserving omni compatibility", "[channel_routing][program_change][player_core]")
{
    PlayerEngine engine;
    prepare (engine, MpeMode::None, R"(
        <region> lochan=1 hichan=1 loprog=5 hiprog=5 sample=*sine
        <region> lochan=2 hichan=2 loprog=7 hiprog=7 sample=*saw
        <region> loprog=7 hiprog=7 sample=*tri
    )");

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, 5), 0);
    midi.addEvent (juce::MidiMessage::programChange (2, 7), 1);
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, juce::uint8 (100)), 2);
    midi.addEvent (juce::MidiMessage::noteOn (2, 61, juce::uint8 (100)), 2);
    process (engine, std::move (midi));
    // Restricted channel 1 (5), restricted channel 2 (7), and two omni (7).
    CHECK (engine.getNumActiveVoices() == 4);
}
