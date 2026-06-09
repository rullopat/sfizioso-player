#include "PlayerEngine.h"

#include <sfizioso.hpp>

namespace samplemachine
{

// ---------------------------------------------------------------------------
// SMPL-85 — OSC introspection client.
//
// The per-CC default / used-set / live value have no direct C++ getter; they
// are reachable only through the engine's OSC client API. The engine
// dispatches these GET messages synchronously: sendMessage() invokes the
// client's receive callback inline on the calling (message) thread before
// returning, so we can request-then-read without any async await. The client
// is RT-safe to call concurrently with renderBlock (that is the whole point
// of the messaging API in sfizz-ui).
// ---------------------------------------------------------------------------
struct PlayerOscQuery
{
    sfizioso::Sfizz::ClientPtr client;
    char        replyType = 0;   // OSC type tag of the last reply ('f','i','b'…)
    float       replyF    = 0.0f;
    long        replyI    = 0;
    std::vector<std::uint8_t> replyBlob;
};

// extern "C" so it matches sfizz_receive_t's C linkage exactly.
extern "C" void playerOscReceive (void* data, int /*delay*/, const char* /*path*/,
                                  const char* sig, const sfizz_arg_t* args)
{
    auto* q = static_cast<PlayerOscQuery*> (data);
    if (q == nullptr)
        return;

    q->replyType = (sig != nullptr && sig[0] != '\0') ? sig[0] : 'N';
    switch (q->replyType)
    {
        case 'f': q->replyF = args[0].f; break;
        case 'd': q->replyF = static_cast<float> (args[0].d); break;
        case 'i': q->replyI = args[0].i; break;
        case 'h': q->replyI = static_cast<long> (args[0].h); break;
        case 'b':
            if (args[0].b != nullptr)
                q->replyBlob.assign (args[0].b->data, args[0].b->data + args[0].b->size);
            break;
        default: break;
    }
}

namespace
{
    // Request a GET on `path` and return true if a usable reply was captured.
    bool oscGet (sfizioso::Sfizz& synth, PlayerOscQuery& q, const juce::String& path)
    {
        if (q.client == nullptr)
            return false;
        q.replyType = 0;
        synth.sendMessage (*q.client, 0, path.toRawUTF8(), "", nullptr);
        return q.replyType != 0 && q.replyType != 'N';
    }
}

PlayerEngine::PlayerEngine()
    : synth (std::make_unique<sfizioso::Sfizz>())
{
}

PlayerEngine::~PlayerEngine() = default;

PlayerOscQuery& PlayerEngine::osc()
{
    if (oscQuery == nullptr)
    {
        oscQuery = std::make_unique<PlayerOscQuery>();
        oscQuery->client = sfizioso::Sfizz::createClient (oscQuery.get());
        if (oscQuery->client != nullptr)
            sfizioso::Sfizz::setReceiveCallback (*oscQuery->client, &playerOscReceive);
    }
    return *oscQuery;
}

void PlayerEngine::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                  MpeMode defaultMpeMode,
                                  int maxVoices)
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
                                                      "Polyphony", 1, maxVoices, 16),
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

void PlayerEngine::setSampleReader (sfz::SampleReader* reader)
{
    synth->setSampleReader (reader);
}

int PlayerEngine::getNumRegions() const          { return synth->getNumRegions(); }
int PlayerEngine::getNumPreloadedSamples() const { return static_cast<int> (synth->getNumPreloadedSamples()); }
int PlayerEngine::getNumActiveVoices() const     { return synth->getNumActiveVoices(); }
int PlayerEngine::getNumMasters() const          { return synth->getNumMasters(); }
int PlayerEngine::getNumGroups() const           { return synth->getNumGroups(); }
int PlayerEngine::getNumCurves() const           { return synth->getNumCurves(); }
int PlayerEngine::getNumVoices() const           { return synth->getNumVoices(); }

bool PlayerEngine::shouldReloadFile()  { return synth->shouldReloadFile(); }
bool PlayerEngine::shouldReloadScala() { return synth->shouldReloadScala(); }

// --- SMPL-86 engine quality -----------------------------------------------
void PlayerEngine::setOversamplingFactor (int factor)
{
    // bool return = "did the factor actually change"; immaterial to the UI.
    synth->setOversamplingFactor (factor);
}
int  PlayerEngine::getOversamplingFactor() const  { return synth->getOversamplingFactor(); }

void PlayerEngine::setPreloadSize (std::uint32_t bytes) { synth->setPreloadSize (bytes); }
std::uint32_t PlayerEngine::getPreloadSize() const     { return synth->getPreloadSize(); }

void PlayerEngine::setSampleQuality (bool freewheeling, int quality)
{
    synth->setSampleQuality (freewheeling ? sfizioso::Sfizz::ProcessFreewheeling
                                          : sfizioso::Sfizz::ProcessLive,
                             quality);
}
int PlayerEngine::getSampleQuality (bool freewheeling) const
{
    return synth->getSampleQuality (freewheeling ? sfizioso::Sfizz::ProcessFreewheeling
                                                 : sfizioso::Sfizz::ProcessLive);
}
void PlayerEngine::setOscillatorQuality (bool freewheeling, int quality)
{
    synth->setOscillatorQuality (freewheeling ? sfizioso::Sfizz::ProcessFreewheeling
                                              : sfizioso::Sfizz::ProcessLive,
                                 quality);
}
void PlayerEngine::setFreewheel (bool enabled)
{
    freewheelEnabled = enabled;
    if (enabled) synth->enableFreeWheeling();
    else         synth->disableFreeWheeling();
}
void PlayerEngine::setSustainCancelsRelease (bool value)
{
    synth->setSustainCancelsRelease (value);
}

// --- SMPL-87 tuning --------------------------------------------------------
bool PlayerEngine::loadScalaFile   (const juce::File& file) { return synth->loadScalaFile (file.getFullPathName().toStdString()); }
bool PlayerEngine::loadScalaString (const juce::String& text) { return synth->loadScalaString (text.toStdString()); }
void  PlayerEngine::setScalaRootKey (int rootKey)   { synth->setScalaRootKey (rootKey); }
int   PlayerEngine::getScalaRootKey() const         { return synth->getScalaRootKey(); }
void  PlayerEngine::setTuningFrequency (float hz)   { synth->setTuningFrequency (hz); }
float PlayerEngine::getTuningFrequency() const      { return synth->getTuningFrequency(); }
void  PlayerEngine::setStretchTuning (float ratio)  { synth->loadStretchTuningByRatio (ratio); }

// --- SMPL-88 keyboard ------------------------------------------------------
void PlayerEngine::allSoundOff() { synth->allSoundOff(); }

std::vector<std::pair<int, juce::String>> PlayerEngine::getKeyLabels() const
{
    std::vector<std::pair<int, juce::String>> out;
    for (const auto& kv : synth->getKeyLabels())
        out.emplace_back (static_cast<int> (kv.first), juce::String::fromUTF8 (kv.second.c_str()));
    return out;
}

// --- SMPL-85 CC controls ---------------------------------------------------
std::vector<CcControl> PlayerEngine::getCcControls()
{
    std::vector<CcControl> out;
    auto& q = osc();
    for (const auto& kv : synth->getCCLabels())
    {
        CcControl c;
        c.number       = static_cast<int> (kv.first);
        c.label        = juce::String::fromUTF8 (kv.second.c_str());
        c.defaultValue = 0.0f;
        if (oscGet (*synth, q, "/cc" + juce::String (c.number) + "/default") && q.replyType == 'f')
            c.defaultValue = q.replyF;
        c.isSwitch     = false; // switch detection deferred (first cut: knobs)
        out.push_back (std::move (c));
    }
    return out;
}

float PlayerEngine::getCcValue (int ccNumber)
{
    auto& q = osc();
    if (oscGet (*synth, q, "/cc" + juce::String (ccNumber) + "/value") && q.replyType == 'f')
        return q.replyF;
    return 0.0f;
}

void PlayerEngine::setCcValue (int ccNumber, float normValue)
{
    // automateHdcc behaves like host automation — the right entry for a
    // user-driven UI move (vs hdcc, a plain set). delay 0 = top of next block.
    synth->automateHdcc (0, ccNumber, juce::jlimit (0.0f, 1.0f, normValue));
}

void PlayerEngine::prepareToPlay (double sr, int samplesPerBlock)
{
    synth->setSampleRate (static_cast<float> (sr));
    synth->setSamplesPerBlock (samplesPerBlock);
    debugSampleRate.store (sr, std::memory_order_relaxed);
    sampleRate.store (sr, std::memory_order_relaxed);
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

// --- SMPL-90 granular MPE bend range + RPN auto-config ---------------------
void PlayerEngine::setMpeBendRanges (float masterSemitones, float perNoteSemitones)
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    synth->setMPEPitchBendRange (masterSemitones, perNoteSemitones);
#else
    juce::ignoreUnused (masterSemitones, perNoteSemitones);
#endif
}

float PlayerEngine::getMpeMasterBendRange() const
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    return synth->getMPEMasterPitchBendRange();
#else
    return 0.0f;
#endif
}

float PlayerEngine::getMpePerNoteBendRange() const
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    return synth->getMPEPerNotePitchBendRange();
#else
    return 0.0f;
#endif
}

bool PlayerEngine::getMpeEnabled() const
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    return synth->getMPEEnabled();
#else
    return false;
#endif
}

void PlayerEngine::setMpeMasterBendAutoConfig (bool enabled)
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    synth->setMPEMasterBendAutoConfigEnabled (enabled);
#else
    juce::ignoreUnused (enabled);
#endif
}

void PlayerEngine::setMpePerNoteBendAutoConfig (bool enabled)
{
#if SAMPLEMACHINE_SFIZZ_HAS_MPE
    synth->setMPEPerNoteBendAutoConfigEnabled (enabled);
#else
    juce::ignoreUnused (enabled);
#endif
}

} // namespace samplemachine
