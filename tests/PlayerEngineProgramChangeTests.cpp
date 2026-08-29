#include <PlayerEngine.h>

#include <juce_audio_basics/juce_audio_basics.h>

#include <catch2/catch_test_macros.hpp>

using namespace samplemachine;

namespace
{
constexpr int kBlockSize = 256;

constexpr const char* kProgramSfz = R"(
<region> key=60 loprog=5 hiprog=5 sample=*sine ampeg_release=0.01
<region> key=61 loprog=7 hiprog=7 sample=*sine ampeg_release=0.01
<region> key=62 loprog=0 hiprog=0 sample=*sine ampeg_release=0.01
<region> key=63 loprog=127 hiprog=127 sample=*sine ampeg_release=0.01
)";

void prepareEngine (PlayerEngine& engine, MpeMode mode = MpeMode::None)
{
    engine.prepareToPlay (44100.0, kBlockSize);
    engine.setMpeMode (mode, 48);
    REQUIRE (engine.loadSfzString (kProgramSfz, "test://program-change.sfz"));
}

void process (PlayerEngine& engine, juce::MidiBuffer midi)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    buffer.clear();
    engine.processBlock (buffer, midi);
}

juce::MidiBuffer programAndNote (int programChannel, int program,
                                 int noteChannel, int note)
{
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (programChannel, program), 0);
    midi.addEvent (juce::MidiMessage::noteOn (
                       noteChannel, note, static_cast<juce::uint8> (100)),
                   1);
    return midi;
}
} // namespace

TEST_CASE ("PlayerEngine leaves non-default loprog regions inactive without Program Change",
           "[player_core][program_change]")
{
    PlayerEngine engine;
    prepareEngine (engine);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (
                       1, 60, static_cast<juce::uint8> (100)),
                   0);
    process (engine, std::move (midi));

    CHECK (engine.getNumActiveVoices() == 0);
}

TEST_CASE ("PlayerEngine forwards global Program Change with MPE disabled",
           "[player_core][program_change]")
{
    PlayerEngine engine;
    prepareEngine (engine);

    SECTION ("Manager channel")
    {
        process (engine, programAndNote (1, 5, 1, 60));
        CHECK (engine.getNumActiveVoices() == 1);
    }

    SECTION ("non-Manager channel is also accepted globally")
    {
        process (engine, programAndNote (2, 5, 1, 60));
        CHECK (engine.getNumActiveVoices() == 1);
    }
}

TEST_CASE ("PlayerEngine preserves Program Change boundary values",
           "[player_core][program_change]")
{
    PlayerEngine engine;
    prepareEngine (engine);

    process (engine, programAndNote (1, 127, 1, 63));
    REQUIRE (engine.getNumActiveVoices() == 1);

    engine.allSoundOff();
    process (engine, programAndNote (1, 0, 1, 62));
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine accepts Manager Program Change in MPE Full",
           "[player_core][program_change][mpe]")
{
    PlayerEngine engine;
    prepareEngine (engine, MpeMode::Full);

    process (engine, programAndNote (1, 5, 2, 60));
    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("PlayerEngine drops Member Program Change in MPE Full",
           "[player_core][program_change][mpe]")
{
    PlayerEngine engine;
    prepareEngine (engine, MpeMode::Full);

    process (engine, programAndNote (2, 5, 2, 60));
    CHECK (engine.getNumActiveVoices() == 0);
}

TEST_CASE ("Dropped MPE Member Program Change does not overwrite global program",
           "[player_core][program_change][mpe]")
{
    PlayerEngine engine;
    prepareEngine (engine, MpeMode::Full);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (1, 5), 0);
    midi.addEvent (juce::MidiMessage::programChange (2, 7), 1);
    midi.addEvent (juce::MidiMessage::noteOn (
                       2, 60, static_cast<juce::uint8> (100)),
                   2);
    process (engine, std::move (midi));

    CHECK (engine.getNumActiveVoices() == 1);
}

TEST_CASE ("RPN 6 MCM enables the Member Program Change filter in the same block",
           "[player_core][program_change][mpe]")
{
    PlayerEngine engine;
    prepareEngine (engine, MpeMode::None);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 101, 0), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 100, 6), 1);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 6, 15), 2);
    midi.addEvent (juce::MidiMessage::programChange (2, 5), 3);
    midi.addEvent (juce::MidiMessage::noteOn (
                       2, 60, static_cast<juce::uint8> (100)),
                   4);
    process (engine, std::move (midi));

    REQUIRE (engine.getMpeEnabled());
    CHECK (engine.getNumActiveVoices() == 0);
}

TEST_CASE ("PlayerEngine debug capture reports incoming Program Change",
           "[player_core][program_change][debug_midi]")
{
    PlayerEngine engine;
    prepareEngine (engine, MpeMode::Full);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::programChange (4, 11), 23);
    process (engine, std::move (midi));

    DebugMidiCapture::Event events[4] {};
    const int count = engine.getDebugMidiCapture().drain (events, 4);
    REQUIRE (count == 1);
    CHECK (events[0].type == DebugMidiCapture::EventType::ProgramChange);
    CHECK (events[0].channel == 3);
    CHECK (events[0].data1 == 11);
    CHECK (events[0].data2 == 0);
    CHECK (events[0].timestampSamples == 23);
}
