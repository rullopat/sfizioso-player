#include "PlayerEngine.h"

#include <sfizioso.hpp>

namespace samplemachine
{

PlayerEngine::PlayerEngine()
    : synth (std::make_unique<sfizioso::Sfizz>())
{
}

PlayerEngine::~PlayerEngine() = default;

void PlayerEngine::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                  MpeMode defaultMpeMode)
{
    // Choice indices intentionally match the MpeMode enum integer values
    // (None=0, Pressure=1, Full=2) so the conversion in parameterChanged
    // is a static_cast rather than a string lookup.
    layout.add (
        std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { PlayerEngineParamIds::gainDb, 1 },
                                                      "Gain (dB)",
                                                      juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f),
                                                      0.0f),
        std::make_unique<juce::AudioParameterInt>    (juce::ParameterID { PlayerEngineParamIds::polyphony, 1 },
                                                      "Polyphony", 1, 64, 16),
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PlayerEngineParamIds::mpeMode, 1 },
                                                      "MPE",
                                                      juce::StringArray { "Off", "Pressure", "Full" },
                                                      static_cast<int> (defaultMpeMode)));
}

void PlayerEngine::connectApvts (juce::AudioProcessorValueTreeState& apvts)
{
    gainDbParam = apvts.getRawParameterValue (PlayerEngineParamIds::gainDb);

    if (auto* poly = apvts.getRawParameterValue (PlayerEngineParamIds::polyphony))
        synth->setNumVoices (static_cast<int> (*poly));
}

bool PlayerEngine::loadSfzFile (const juce::File& file)
{
    return synth->loadSfzFile (file.getFullPathName().toStdString());
}

bool PlayerEngine::loadSfzString (const juce::String& sfzText, const juce::String& virtualPath)
{
    return synth->loadSfzString (virtualPath.toStdString(), sfzText.toStdString());
}

int PlayerEngine::getNumRegions() const          { return synth->getNumRegions(); }
int PlayerEngine::getNumPreloadedSamples() const { return static_cast<int> (synth->getNumPreloadedSamples()); }
int PlayerEngine::getNumActiveVoices() const     { return synth->getNumActiveVoices(); }

void PlayerEngine::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth->setSampleRate (static_cast<float> (sampleRate));
    synth->setSamplesPerBlock (samplesPerBlock);
    debugSampleRate.store (sampleRate, std::memory_order_relaxed);
}

void PlayerEngine::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (gainDbParam != nullptr)
        synth->setVolume (gainDbParam->load());

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const int delay = metadata.samplePosition;
        // JUCE channels are 1..16; sfizz channels are 0..15. msg.getChannel()
        // returns 0 for system messages (no channel) — those are filtered out
        // by the per-event isFoo() guards below, so the subtraction only runs
        // for channel-bearing messages.
        const int channel = juce::jmax (0, msg.getChannel() - 1);

        auto pushDebug = [&] (DebugMidiCapture::EventType t, int d1, int d2)
        {
            debugMidi.push ({ debugSampleClock + delay,
                              t,
                              static_cast<std::uint8_t> (channel),
                              d1, d2 });
        };
        if      (msg.isNoteOn())          pushDebug (DebugMidiCapture::EventType::NoteOn,          msg.getNoteNumber(),       msg.getVelocity());
        else if (msg.isNoteOff())         pushDebug (DebugMidiCapture::EventType::NoteOff,         msg.getNoteNumber(),       msg.getVelocity());
        else if (msg.isController())      pushDebug (DebugMidiCapture::EventType::CC,              msg.getControllerNumber(), msg.getControllerValue());
        else if (msg.isPitchWheel())      pushDebug (DebugMidiCapture::EventType::PitchBend,       msg.getPitchWheelValue() - 8192, 0);
        else if (msg.isChannelPressure()) pushDebug (DebugMidiCapture::EventType::ChannelPressure, msg.getChannelPressureValue(),   0);
        else if (msg.isAftertouch())      pushDebug (DebugMidiCapture::EventType::PolyAftertouch,  msg.getNoteNumber(),       msg.getAfterTouchValue());

        // Dispatch unconditionally through the *MPE entry points. The engine
        // normalizes channel to 0 internally when MPE is disabled (see
        // sfz::Synth::setMPEEnabled), so MpeMode::None / Pressure callers
        // get pre-fork single-channel behaviour without an extra branch here.
        if (msg.isNoteOn())
            synth->noteOn (delay, channel, msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            synth->noteOff (delay, channel, msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isController())
            synth->cc (delay, channel, msg.getControllerNumber(), msg.getControllerValue());
        else if (msg.isPitchWheel())
            synth->pitchWheel (delay, channel, msg.getPitchWheelValue() - 8192);
        else if (msg.isChannelPressure())
            synth->channelAftertouch (delay, channel, msg.getChannelPressureValue());
        else if (msg.isAftertouch())
            synth->polyAftertouch (delay, channel, msg.getNoteNumber(), msg.getAfterTouchValue());
    }

    float* outputs[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    synth->renderBlock (outputs, static_cast<size_t> (buffer.getNumSamples()), 1);

    debugSampleClock += buffer.getNumSamples();
}

void PlayerEngine::setNumVoices (int n)
{
    synth->setNumVoices (n);
}

void PlayerEngine::setMpeMode (MpeMode mode, int perNoteBendRangeSemitones)
{
    mpeMode = mode;
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    synth->setMPEEnabled (mode == MpeMode::Full);
    if (mode == MpeMode::Full)
        synth->setMPEPitchBendRange (synth->getMPEMasterPitchBendRange(),
                                     static_cast<float> (perNoteBendRangeSemitones));
#else
    juce::ignoreUnused (perNoteBendRangeSemitones);
    jassert (mode != MpeMode::Full); // build is not the SMPL-29 fork
#endif
}

} // namespace samplemachine
