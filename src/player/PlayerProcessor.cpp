#include "PlayerProcessor.h"

#include "PlayerEditor.h"

namespace samplemachine
{

// MPE 1.0 default per-note pitch bend range (semitones). Mirrors the
// rompler's manifest-driven kDefaultBendRange but lives as a constant
// here because the generic player has no per-binary brand config.
static constexpr int kPlayerDefaultMpeBendRange = 48;

PlayerProcessor::PlayerProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    if (! apvts.state.hasProperty (PlayerStateProps::sfzPath))
        apvts.state.setProperty (PlayerStateProps::sfzPath, juce::String(), nullptr);

    engine.connectApvts (apvts);

    // Apply the initial APVTS-resident mpeMode value — defaults to Full
    // per createParameterLayout(). Saved DAW state will overwrite via
    // setStateInformation and the listener picks that up.
    if (auto* raw = apvts.getRawParameterValue (PlayerEngineParamIds::mpeMode))
        engine.setMpeMode (static_cast<MpeMode> (static_cast<int> (raw->load())),
                           kPlayerDefaultMpeBendRange);

    apvts.addParameterListener (PlayerEngineParamIds::polyphony, this);
    apvts.addParameterListener (PlayerEngineParamIds::mpeMode,   this);
}

PlayerProcessor::~PlayerProcessor()
{
    apvts.removeParameterListener (PlayerEngineParamIds::polyphony, this);
    apvts.removeParameterListener (PlayerEngineParamIds::mpeMode,   this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PlayerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Generic player defaults to full MPE: non-MPE keyboards send on
    // channel 1 which collapses to master-channel inheritance and renders
    // identically to None, while MPE controllers just work out of the
    // box. SMPL-39.
    PlayerEngine::addParameters (layout, MpeMode::Full);
    return layout;
}

bool PlayerProcessor::loadSfzFile (const juce::File& file)
{
    const bool ok = engine.loadSfzFile (file);
    if (ok)
        apvts.state.setProperty (PlayerStateProps::sfzPath, file.getFullPathName(), nullptr);
    return ok;
}

juce::File PlayerProcessor::getCurrentSfzFile() const
{
    const auto path = apvts.state.getProperty (PlayerStateProps::sfzPath).toString();
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

void PlayerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepareToPlay (sampleRate, samplesPerBlock);
}

void PlayerProcessor::releaseResources() {}

bool PlayerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PlayerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    engine.processBlock (buffer, midi);

    // SMPL-56: track post-engine peak magnitude per channel for the editor-side
    // meter. Compare-exchange in a loop so the editor reads the maximum value
    // observed across blocks since the last consumePeak* read.
    auto storeMax = [] (std::atomic<float>& target, float value) noexcept
    {
        float prev = target.load (std::memory_order_relaxed);
        while (value > prev
               && ! target.compare_exchange_weak (prev, value,
                                                  std::memory_order_relaxed,
                                                  std::memory_order_relaxed))
        { /* prev updated by compare_exchange_weak; retry */ }
    };

    const auto numCh    = buffer.getNumChannels();
    const auto numSamps = buffer.getNumSamples();
    if (numCh > 0)
        storeMax (peakL, buffer.getMagnitude (0, 0, numSamps));
    if (numCh > 1)
        storeMax (peakR, buffer.getMagnitude (1, 0, numSamps));
    else
        storeMax (peakR, peakL.load (std::memory_order_relaxed));
}

juce::AudioProcessorEditor* PlayerProcessor::createEditor()
{
    return new PlayerEditor (*this);
}

void PlayerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void PlayerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            const auto path = apvts.state.getProperty (PlayerStateProps::sfzPath).toString();
            if (path.isNotEmpty())
            {
                juce::File file (path);
                if (file.existsAsFile())
                    engine.loadSfzFile (file);
            }
        }
    }
}

void PlayerProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == PlayerEngineParamIds::polyphony)
    {
        // sfizz::setNumVoices requires no concurrent RT calls — suspend processing
        // around the change so the audio thread is guaranteed not to be inside sfizz.
        suspendProcessing (true);
        engine.setNumVoices (static_cast<int> (newValue));
        suspendProcessing (false);
    }
    else if (parameterID == PlayerEngineParamIds::mpeMode)
    {
        // sfizz::setMPEEnabled is not RT-safe — suspend so the audio
        // thread is out of the engine while it reconfigures.
        suspendProcessing (true);
        engine.setMpeMode (static_cast<MpeMode> (static_cast<int> (newValue)),
                           kPlayerDefaultMpeBendRange);
        suspendProcessing (false);
    }
}

} // namespace samplemachine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new samplemachine::PlayerProcessor();
}
