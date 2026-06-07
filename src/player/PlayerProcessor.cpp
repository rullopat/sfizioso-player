#include "PlayerProcessor.h"

#include "PlayerEditor.h"

#include <PreferenceStore.h>

namespace samplemachine
{

// MPE 1.0 default per-note pitch bend range (semitones). Mirrors the
// rompler's manifest-driven kDefaultBendRange but lives as a constant
// here because the generic player has no per-binary brand config.
static constexpr int kPlayerDefaultMpeBendRange = 48;

// SMPL-89 — recent-files persistence (global, cross-session) via the shared
// PreferenceStore, distinct from the per-instance sfzPath in DAW state.
static constexpr const char* kRecentFilesKey = "player_recent_files";
static constexpr int kMaxRecentFiles = 8;

// SMPL-87 — embedded 12-TET scale used to reset to default temperament; the
// engine has no "unload scala", so reset = load standard equal temperament.
static const char* kDefaultScala =
    "! 12-TET\n"
    "12-tone equal temperament\n"
    " 12\n"
    "!\n"
    " 100.0\n 200.0\n 300.0\n 400.0\n 500.0\n 600.0\n"
    " 700.0\n 800.0\n 900.0\n 1000.0\n 1100.0\n 2/1\n";

PlayerProcessor::PlayerProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    if (! apvts.state.hasProperty (PlayerStateProps::sfzPath))
        apvts.state.setProperty (PlayerStateProps::sfzPath, juce::String(), nullptr);
    if (! apvts.state.hasProperty (PlayerStateProps::scalaPath))
        apvts.state.setProperty (PlayerStateProps::scalaPath, juce::String(), nullptr);

    engine.connectApvts (apvts);

    // Apply the initial APVTS-resident mpeMode value — defaults to Full
    // per createParameterLayout(). Saved DAW state will overwrite via
    // setStateInformation and the listener picks that up.
    if (auto* raw = apvts.getRawParameterValue (PlayerEngineParamIds::mpeMode))
        engine.setMpeMode (static_cast<MpeMode> (static_cast<int> (raw->load())),
                           kPlayerDefaultMpeBendRange);

    applyEngineSettings();

    for (const char* id : { PlayerEngineParamIds::polyphony,
                            PlayerEngineParamIds::mpeMode,
                            PlayerEngineParamIds::oversampling,
                            PlayerEngineParamIds::preloadSize,
                            PlayerEngineParamIds::sampleQualityLive,
                            PlayerEngineParamIds::sampleQualityFreewheel,
                            PlayerEngineParamIds::freewheel,
                            PlayerEngineParamIds::scalaRootKey,
                            PlayerEngineParamIds::tuningFrequency,
                            PlayerEngineParamIds::stretchTuning })
        apvts.addParameterListener (id, this);
}

PlayerProcessor::~PlayerProcessor()
{
    for (const char* id : { PlayerEngineParamIds::polyphony,
                            PlayerEngineParamIds::mpeMode,
                            PlayerEngineParamIds::oversampling,
                            PlayerEngineParamIds::preloadSize,
                            PlayerEngineParamIds::sampleQualityLive,
                            PlayerEngineParamIds::sampleQualityFreewheel,
                            PlayerEngineParamIds::freewheel,
                            PlayerEngineParamIds::scalaRootKey,
                            PlayerEngineParamIds::tuningFrequency,
                            PlayerEngineParamIds::stretchTuning })
        apvts.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PlayerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // Generic player defaults to full MPE: non-MPE keyboards send on
    // channel 1 which collapses to master-channel inheritance and renders
    // identically to None, while MPE controllers just work out of the box.
    PlayerEngine::addParameters (layout, MpeMode::Full);

    using namespace PlayerEngineParamIds;

    // SMPL-86 engine / quality. Registered here (player-app-only) rather than
    // in the shared PlayerEngine::addParameters, so the closed romplers'
    // parameter list is unchanged.
    layout.add (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { oversampling, 1 },
                                                      "Oversampling",
                                                      juce::StringArray { "1x", "2x", "4x", "8x" }, 0),
        // Preload size in kB (engine takes bytes; we multiply). Default 8 kB
        // matches the engine's 8192-byte default.
        std::make_unique<juce::AudioParameterInt>    (juce::ParameterID { preloadSize, 1 },
                                                      "Preload (kB)", 4, 1024, 8),
        std::make_unique<juce::AudioParameterInt>    (juce::ParameterID { sampleQualityLive, 1 },
                                                      "Sample Quality (Live)", 0, 10, 2),
        std::make_unique<juce::AudioParameterInt>    (juce::ParameterID { sampleQualityFreewheel, 1 },
                                                      "Sample Quality (Freewheel)", 0, 10, 10),
        std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { freewheel, 1 },
                                                      "Freewheel", false));

    // SMPL-87 tuning.
    layout.add (
        std::make_unique<juce::AudioParameterInt>    (juce::ParameterID { scalaRootKey, 1 },
                                                      "Scale Root Key", 0, 127, 60),
        std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { tuningFrequency, 1 },
                                                      "Tuning Frequency",
                                                      juce::NormalisableRange<float> (380.0f, 480.0f, 0.1f),
                                                      440.0f),
        std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { stretchTuning, 1 },
                                                      "Stretch Tuning",
                                                      juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                      0.0f));
    return layout;
}

void PlayerProcessor::applyEngineSettings()
{
    auto valInt = [this] (const char* id) -> int
    {
        if (auto* r = apvts.getRawParameterValue (id)) return static_cast<int> (r->load());
        return 0;
    };
    auto valFloat = [this] (const char* id) -> float
    {
        if (auto* r = apvts.getRawParameterValue (id)) return r->load();
        return 0.0f;
    };

    engine.setOversamplingFactor (1 << valInt (PlayerEngineParamIds::oversampling)); // 0..3 -> 1/2/4/8
    engine.setPreloadSize (static_cast<std::uint32_t> (valInt (PlayerEngineParamIds::preloadSize)) * 1024u);
    engine.setSampleQuality (false, valInt (PlayerEngineParamIds::sampleQualityLive));
    engine.setSampleQuality (true,  valInt (PlayerEngineParamIds::sampleQualityFreewheel));
    engine.setFreewheel (valInt (PlayerEngineParamIds::freewheel) != 0);

    engine.setScalaRootKey (valInt (PlayerEngineParamIds::scalaRootKey));
    engine.setTuningFrequency (valFloat (PlayerEngineParamIds::tuningFrequency));
    engine.setStretchTuning (valFloat (PlayerEngineParamIds::stretchTuning));

    // Restore a previously-loaded Scala scale from session state, else 12-TET.
    const auto scala = apvts.state.getProperty (PlayerStateProps::scalaPath).toString();
    if (scala.isNotEmpty() && juce::File (scala).existsAsFile())
        loadScalaFile (juce::File (scala));
}

bool PlayerProcessor::loadSfzFile (const juce::File& file)
{
    suspendProcessing (true);
    const bool ok = engine.loadSfzFile (file);
    suspendProcessing (false);
    if (ok)
    {
        apvts.state.setProperty (PlayerStateProps::sfzPath, file.getFullPathName(), nullptr);
        lastSfzModTime = file.getLastModificationTime();
        appendRecentFile (file);
    }
    return ok;
}

juce::File PlayerProcessor::getCurrentSfzFile() const
{
    const auto path = apvts.state.getProperty (PlayerStateProps::sfzPath).toString();
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

// --- SMPL-87 scala ---------------------------------------------------------
bool PlayerProcessor::loadScalaFile (const juce::File& file)
{
    suspendProcessing (true);
    const bool ok = engine.loadScalaFile (file);
    suspendProcessing (false);
    if (ok)
    {
        apvts.state.setProperty (PlayerStateProps::scalaPath, file.getFullPathName(), nullptr);
        currentScalaName = file.getFileNameWithoutExtension();
    }
    return ok;
}

bool PlayerProcessor::resetScala()
{
    suspendProcessing (true);
    const bool ok = engine.loadScalaString (juce::String (kDefaultScala));
    suspendProcessing (false);
    apvts.state.setProperty (PlayerStateProps::scalaPath, juce::String(), nullptr);
    currentScalaName = "12-TET / Default";
    return ok;
}

juce::String PlayerProcessor::getScalaName() const { return currentScalaName; }

// --- SMPL-89 recent files + auto-reload ------------------------------------
juce::StringArray PlayerProcessor::getRecentFiles() const
{
    juce::StringArray out;
    const auto raw = PreferenceStore::get (kRecentFilesKey, "[]");
    auto parsed = juce::JSON::parse (raw);
    if (auto* arr = parsed.getArray())
        for (const auto& v : *arr)
        {
            const auto p = v.toString();
            if (p.isNotEmpty() && juce::File (p).existsAsFile())
                out.add (p);
        }
    return out;
}

void PlayerProcessor::appendRecentFile (const juce::File& file)
{
    auto list = getRecentFiles();
    const auto path = file.getFullPathName();
    list.removeString (path);
    list.insert (0, path);
    while (list.size() > kMaxRecentFiles)
        list.remove (list.size() - 1);

    juce::Array<juce::var> arr;
    for (const auto& p : list)
        arr.add (p);
    PreferenceStore::set (kRecentFilesKey, juce::JSON::toString (juce::var (arr), true));
}

bool PlayerProcessor::checkForFileReload()
{
    const auto file = getCurrentSfzFile();
    if (! file.existsAsFile())
        return false;

    bool changed = engine.shouldReloadFile(); // tracks changed includes
    const auto mod = file.getLastModificationTime();
    if (mod != lastSfzModTime)
        changed = true;
    if (! changed)
        return false;

    suspendProcessing (true);
    const bool ok = engine.loadSfzFile (file);
    suspendProcessing (false);
    lastSfzModTime = mod;
    return ok;
}

bool PlayerProcessor::checkForScalaReload()
{
    if (! engine.shouldReloadScala())
        return false;
    const auto path = apvts.state.getProperty (PlayerStateProps::scalaPath).toString();
    if (path.isEmpty())
        return false;
    return loadScalaFile (juce::File (path));
}

// --- SMPL-88 note injection ------------------------------------------------
void PlayerProcessor::injectNote (bool noteOn, int noteNumber, int velocity)
{
    int start1, size1, start2, size2;
    noteFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
        noteRing[(size_t) start1] = { noteOn, noteNumber, velocity };
    else if (size2 > 0)
        noteRing[(size_t) start2] = { noteOn, noteNumber, velocity };
    noteFifo.finishedWrite (size1 + size2);
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
    // SMPL-88 — drain UI-injected notes into the MIDI stream at block start so
    // they ride the same MPE dispatch path as host MIDI.
    if (const int ready = noteFifo.getNumReady(); ready > 0)
    {
        int start1, size1, start2, size2;
        noteFifo.prepareToRead (ready, start1, size1, start2, size2);
        auto add = [&] (const InjectedNote& n)
        {
            auto m = n.on ? juce::MidiMessage::noteOn  (1, n.note, static_cast<juce::uint8> (n.velocity))
                          : juce::MidiMessage::noteOff (1, n.note);
            midi.addEvent (m, 0);
        };
        for (int i = 0; i < size1; ++i) add (noteRing[(size_t) (start1 + i)]);
        for (int i = 0; i < size2; ++i) add (noteRing[(size_t) (start2 + i)]);
        noteFifo.finishedRead (size1 + size2);
    }

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
                {
                    suspendProcessing (true);
                    engine.loadSfzFile (file);
                    suspendProcessing (false);
                    lastSfzModTime = file.getLastModificationTime();
                }
            }

            // Re-assert APVTS → engine for the no-getter settings (freewheel,
            // tuning, quality) and restore any saved Scala scale.
            applyEngineSettings();
        }
    }
}

void PlayerProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    using namespace PlayerEngineParamIds;

    // RT-safe scalar setters — no suspend needed.
    if (parameterID == tuningFrequency)        { engine.setTuningFrequency (newValue); return; }
    if (parameterID == stretchTuning)          { engine.setStretchTuning (newValue);   return; }
    if (parameterID == scalaRootKey)           { engine.setScalaRootKey (static_cast<int> (newValue)); return; }

    // Non-RT-safe — bracket so the audio thread is guaranteed out of sfizz.
    suspendProcessing (true);
    if (parameterID == polyphony)
        engine.setNumVoices (static_cast<int> (newValue));
    else if (parameterID == mpeMode)
        engine.setMpeMode (static_cast<MpeMode> (static_cast<int> (newValue)), kPlayerDefaultMpeBendRange);
    else if (parameterID == oversampling)
        engine.setOversamplingFactor (1 << static_cast<int> (newValue));
    else if (parameterID == preloadSize)
        engine.setPreloadSize (static_cast<std::uint32_t> (newValue) * 1024u);
    else if (parameterID == sampleQualityLive)
        engine.setSampleQuality (false, static_cast<int> (newValue));
    else if (parameterID == sampleQualityFreewheel)
        engine.setSampleQuality (true, static_cast<int> (newValue));
    else if (parameterID == freewheel)
        engine.setFreewheel (newValue > 0.5f);
    suspendProcessing (false);
}

} // namespace samplemachine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new samplemachine::PlayerProcessor();
}
