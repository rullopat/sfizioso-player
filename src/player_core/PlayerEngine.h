#pragma once

#include "DebugMidiCapture.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

namespace sfz { class Sfizz; }
namespace sfizioso { using Sfizz = ::sfz::Sfizz; } // brand alias; full surface in <sfizioso.hpp>

namespace samplemachine
{

namespace PlayerEngineParamIds
{
    inline constexpr const char* gainDb    = "gainDb";
    inline constexpr const char* polyphony = "polyphony";
    inline constexpr const char* mpeMode   = "mpeMode";
}

/**
 * @brief How the engine should treat per-channel MIDI input.
 *
 * - None      single-channel; collapses everything to channel 0 (master).
 *             Default. Behavior matches the pre-MPE engine.
 * - Pressure  same dispatch as None today (placeholder; SMPL-28 v1's
 *             channel-pressure-to-polyAT translation lives here in a
 *             follow-up). Brands set "pressure" when they want some
 *             expressivity but don't need full per-note pitch.
 * - Full      true MPE — pitch / CC / aftertouch arriving on member
 *             channels (1..15) are routed to per-channel modulation
 *             slots so voices respond independently. Requires the
 *             SMPL-29 sfizz fork (SAMPLEMACHINE_SFIZZ_HAS_MPE).
 */
enum class MpeMode { None, Pressure, Full };

class PlayerEngine
{
public:
    PlayerEngine();
    ~PlayerEngine();

    /**
     * @brief Register the engine's APVTS parameters into a host's layout.
     *
     * @param defaultMpeMode  Initial value for the "mpeMode" choice param.
     *                        The host shell passes the per-brand default
     *                        (e.g. RomplerProcessor reads it from
     *                        BrandConfig::kMpeMode); the user can flip
     *                        to any of the three values at runtime via
     *                        the editor combo box, and DAW project state
     *                        round-trips the chosen value through APVTS.
     */
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                               MpeMode defaultMpeMode = MpeMode::None);

    void connectApvts (juce::AudioProcessorValueTreeState& apvts);

    bool loadSfzFile   (const juce::File& file);
    bool loadSfzString (const juce::String& sfzText, const juce::String& virtualPath);

    int getNumRegions() const;
    int getNumPreloadedSamples() const;
    int getNumActiveVoices() const;

    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void processBlock  (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    void setNumVoices (int n);

    /**
     * @brief Set the MPE handling mode and (optionally) the per-note pitch
     * bend range in semitones. Call once at construction from the host
     * shell (RomplerProcessor reads it from BrandConfig). Defaults are
     * Mode::None / 48 semitones per-note (MPE 1.0 convention) but only
     * matter when mode is Full.
     */
    void setMpeMode (MpeMode mode, int perNoteBendRangeSemitones = 48);
    MpeMode getMpeMode() const noexcept { return mpeMode; }

    /** UI access for the in-plugin MIDI debug panel (test brands only). */
    DebugMidiCapture& getDebugMidiCapture() noexcept { return debugMidi; }
    double            getDebugSampleRate() const noexcept { return debugSampleRate.load (std::memory_order_relaxed); }

private:
    std::unique_ptr<sfizioso::Sfizz> synth;
    std::atomic<float>* gainDbParam = nullptr;
    MpeMode mpeMode = MpeMode::None;

    DebugMidiCapture     debugMidi;
    std::int64_t         debugSampleClock = 0;
    std::atomic<double>  debugSampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEngine)
};

} // namespace samplemachine
