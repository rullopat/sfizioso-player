#pragma once

#include "PlayerEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace samplemachine
{

namespace PlayerStateProps
{
    inline constexpr const char* sfzPath   = "sfzPath";
    inline constexpr const char* scalaPath = "scalaPath"; // SMPL-87
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
    int getNumMasters() const            { return engine.getNumMasters(); }
    int getNumGroups() const             { return engine.getNumGroups(); }
    int getNumCurves() const             { return engine.getNumCurves(); }
    int getNumVoices() const             { return engine.getNumVoices(); }
    // Sample rate uses juce::AudioProcessor::getSampleRate() (host-prepared);
    // the engine mirrors it too (PlayerEngine::getSampleRate) if needed.

    PlayerEngine& getEngine() noexcept { return engine; }

    // SMPL-87 — tuning. Scala loads bracket suspendProcessing (parse-state
    // mutation, same constraint as SFZ load).
    bool loadScalaFile (const juce::File& file);
    bool resetScala();   // load embedded 12-TET default
    juce::String getScalaName() const;

    // SMPL-89 — recent files + on-disk auto-reload.
    juce::StringArray getRecentFiles() const;     // most-recent-first, pruned
    bool checkForFileReload();                     // returns true if it reloaded
    bool checkForScalaReload();

    // SMPL-88 — enqueue an on-screen-keyboard note (message thread / producer).
    // The audio thread drains the queue at the top of processBlock and replays
    // events through the same MPE dispatch path as host MIDI.
    void injectNote (bool noteOn, int noteNumber, int velocity);

    // Reads-and-clears the max-since-last-call peak magnitude, in [0..1].
    // SMPL-56: drives the React-side level meter via the editor's "meter" event.
    float consumePeakL() noexcept        { return peakL.exchange (0.0f, std::memory_order_relaxed); }
    float consumePeakR() noexcept        { return peakR.exchange (0.0f, std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

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

    // Push the current APVTS values for the SMPL-86 / SMPL-87 settings into the
    // engine. Called once after construction and again after state restore so
    // APVTS is the authoritative source for the non-getter-backed settings.
    void applyEngineSettings();
    void appendRecentFile (const juce::File& file);

    PlayerEngine engine;
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    // SMPL-88 — lock-free SPSC note-injection queue (UI producer / audio
    // consumer). Fixed ring; overflow silently drops (a dropped UI key press
    // is benign and can't happen at realistic click rates).
    struct InjectedNote { bool on; int note; int velocity; };
    static constexpr int kNoteFifoSize = 512;
    juce::AbstractFifo noteFifo { kNoteFifoSize };
    std::array<InjectedNote, kNoteFifoSize> noteRing {};

    juce::Time lastSfzModTime;   // SMPL-89 host-side mtime watch
    juce::String currentScalaName { "12-TET / Default" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerProcessor)
};

} // namespace samplemachine
