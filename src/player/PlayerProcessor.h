#pragma once

#include "PlayerEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace samplemachine
{

namespace PlayerStateProps
{
    inline constexpr const char* sfzPath = "sfzPath";
}

class PlayerProcessor : public juce::AudioProcessor,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    PlayerProcessor();
    ~PlayerProcessor() override;

    bool loadSfzFile (const juce::File& file);
    juce::File getCurrentSfzFile() const;

    int getNumRegions() const            { return engine.getNumRegions(); }
    int getNumPreloadedSamples() const   { return engine.getNumPreloadedSamples(); }
    int getNumActiveVoices() const       { return engine.getNumActiveVoices(); }

    // Reads-and-clears the max-since-last-call peak magnitude, in [0..1].
    // SMPL-56: drives the React-side level meter via the editor's "meter" event.
    float consumePeakL() noexcept        { return peakL.exchange (0.0f, std::memory_order_relaxed); }
    float consumePeakR() noexcept        { return peakR.exchange (0.0f, std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    PlayerEngine& getEngine() noexcept { return engine; }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    PlayerEngine engine;
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerProcessor)
};

} // namespace samplemachine
