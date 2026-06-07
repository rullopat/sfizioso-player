#pragma once

#include "DebugMidiCapture.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace sfz { class Sfizz; }
namespace sfizioso { using Sfizz = ::sfz::Sfizz; } // brand alias; full surface in <sfizioso.hpp>

namespace samplemachine
{

// SMPL-85 — opaque holder for the OSC introspection client + synchronous
// reply capture. Defined in PlayerEngine.cpp so the engine's sfizz client
// types stay out of this shared header.
struct PlayerOscQuery;

namespace PlayerEngineParamIds
{
    inline constexpr const char* gainDb    = "gainDb";
    inline constexpr const char* polyphony = "polyphony";
    inline constexpr const char* mpeMode   = "mpeMode";

    // SMPL-86 engine / quality. Registered only by the Sfizioso Player app
    // (PlayerProcessor::createParameterLayout), NOT by the shared
    // addParameters() below — so the closed romplers' parameter list is
    // unchanged. The constants live here so the player editor relays and
    // the engine setters reference one source of truth.
    inline constexpr const char* oversampling           = "oversampling";
    inline constexpr const char* preloadSize            = "preloadSize";
    inline constexpr const char* sampleQualityLive      = "sampleQualityLive";
    inline constexpr const char* sampleQualityFreewheel = "sampleQualityFreewheel";
    inline constexpr const char* freewheel              = "freewheel";

    // SMPL-87 tuning. Player-app-only registration (see note above).
    inline constexpr const char* scalaRootKey    = "scalaRootKey";
    inline constexpr const char* tuningFrequency = "tuningFrequency";
    inline constexpr const char* stretchTuning   = "stretchTuning";

    // SMPL-90 MPE settings (player-app-only registration; the rompler keeps
    // its single mpeMode + manifest bend range).
    inline constexpr const char* mpeMasterBend       = "mpeMasterBend";
    inline constexpr const char* mpePerNoteBend       = "mpePerNoteBend";
    inline constexpr const char* mpeIgnoreMasterRpn  = "mpeIgnoreMasterRpn";
    inline constexpr const char* mpeIgnorePerNoteRpn = "mpeIgnorePerNoteRpn";
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

/**
 * @brief One auto-generated CC control for the editor's CONTROLS panel.
 *
 * SMPL-85. Built from the engine's direct CC-label getter joined with the
 * per-CC default value read over the OSC client introspection API.
 */
struct CcControl
{
    int          number       = 0;
    juce::String label;
    float        defaultValue = 0.0f; // normalised 0..1
    bool         isSwitch     = false;
};

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

    // SMPL-83 — remaining read-only parse/runtime scalars for the info panel.
    int    getNumMasters() const;
    int    getNumGroups() const;
    int    getNumCurves() const;
    int    getNumVoices() const;
    // Host-side sample rate captured in prepareToPlay (the engine exposes no
    // getSampleRate). 0 until prepared.
    double getSampleRate() const noexcept { return sampleRate.load (std::memory_order_relaxed); }

    // SMPL-89 — on-disk change detection. May open file descriptors → call
    // from a low-rate control/message-thread timer, never the audio thread.
    bool shouldReloadFile();

    // SMPL-86 — engine quality / performance. All non-RT-safe (realloc /
    // disk re-read): callers must bracket with suspendProcessing.
    void     setOversamplingFactor (int factor); // 1 / 2 / 4 / 8
    int      getOversamplingFactor() const;
    void     setPreloadSize (std::uint32_t bytes);
    std::uint32_t getPreloadSize() const;
    void     setSampleQuality (bool freewheeling, int quality); // 0..10
    int      getSampleQuality (bool freewheeling) const;
    void     setFreewheel (bool enabled);

    // SMPL-87 — Scala / tuning. Scala loads mutate parse state → bracket with
    // suspendProcessing like the SFZ load. The scalar setters are low-rate.
    bool  loadScalaFile   (const juce::File& file);
    bool  loadScalaString (const juce::String& text);
    void  setScalaRootKey (int rootKey);
    int   getScalaRootKey() const;
    void  setTuningFrequency (float hz);
    float getTuningFrequency() const;
    void  setStretchTuning (float ratio); // 0..1; 0 disables
    bool  shouldReloadScala();

    // SMPL-88 — on-screen keyboard support.
    void allSoundOff();
    /** Mapped/labelled keys (key number + optional label_key text), refreshed
     *  on each SFZ (re)load. Empty when the instrument exposes no key labels. */
    std::vector<std::pair<int, juce::String>> getKeyLabels() const;

    // SMPL-85 — CC controls (CONTROLS panel). getCcControls joins the direct
    // label getter with per-CC OSC default queries; getCcValue / setCcValue
    // are the two-way binding. All run on the message thread (the OSC client
    // dispatch is synchronous and RT-safe).
    std::vector<CcControl> getCcControls();
    float getCcValue (int ccNumber);
    void  setCcValue (int ccNumber, float normValue);

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

    /**
     * @brief SMPL-90 — granular MPE bend-range + RPN auto-config, for the
     * player's MPE panel. Additive over setMpeMode (which the rompler uses
     * unchanged). All no-ops on a non-fork build, like setMpeMode.
     *  - setMpeBendRanges sets the master / per-note pitch-bend range (st).
     *  - the AutoConfig setters gate whether incoming RPN 0 (Pitch Bend
     *    Sensitivity) updates the master / per-note range (default: enabled).
     */
    void  setMpeBendRanges (float masterSemitones, float perNoteSemitones);
    float getMpeMasterBendRange() const;
    float getMpePerNoteBendRange() const;
    bool  getMpeEnabled() const;
    void  setMpeMasterBendAutoConfig (bool enabled);
    void  setMpePerNoteBendAutoConfig (bool enabled);

    /** UI access for the in-plugin MIDI debug panel (test brands only). */
    DebugMidiCapture& getDebugMidiCapture() noexcept { return debugMidi; }
    double            getDebugSampleRate() const noexcept { return debugSampleRate.load (std::memory_order_relaxed); }

private:
    // Lazily-created OSC introspection client (SMPL-85). osc() builds it on
    // first use against the live synth.
    PlayerOscQuery& osc();

    std::unique_ptr<sfizioso::Sfizz> synth;
    std::unique_ptr<PlayerOscQuery> oscQuery;
    std::atomic<float>* gainDbParam = nullptr;
    MpeMode mpeMode = MpeMode::None;
    bool freewheelEnabled = false; // engine has no freewheel-state getter

    DebugMidiCapture     debugMidi;
    std::int64_t         debugSampleClock = 0;
    std::atomic<double>  debugSampleRate { 0.0 };
    std::atomic<double>  sampleRate { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEngine)
};

} // namespace samplemachine
