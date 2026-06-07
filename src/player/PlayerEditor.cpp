#include "PlayerEditor.h"
#include "PlayerProcessor.h"

#include "BinaryData.h"

namespace samplemachine
{

namespace
{
    // SMPL-82 — the multi-panel shell (CONTROLS / OUTPUT+ENGINE+TUNING / INFO
    // over a full-width KEYBOARD row) needs far more room than the original
    // single OUTPUT strip. The editor is resizable; the React layout is a
    // responsive CSS grid, so it reflows to whatever size the user picks.
    constexpr int kEditorBaseWidth  = 1180;
    constexpr int kEditorHeight     = 760;
    constexpr int kEditorMinWidth   = 1000;
    constexpr int kEditorMinHeight  = 680;
    constexpr int kDebugPanelWidth  = 360;
    // 30 Hz keeps the meter responsive without flooding the message thread;
    // the active-voices readout is fine at the same rate.
    constexpr int kTelemetryTimerHz = 30;
    // Derived sub-rates (in timer ticks): CC reflect-back ~10 Hz, the
    // file/scala on-disk reload poll ~2 Hz (shouldReloadFile may open FDs).
    constexpr int kCcPollEvery     = 3;
    constexpr int kReloadPollEvery = 15;

    juce::String mimeForExtension (const juce::String& ext)
    {
        if (ext == "html")  return "text/html";
        if (ext == "js")    return "application/javascript";
        if (ext == "css")   return "text/css";
        if (ext == "svg")   return "image/svg+xml";
        if (ext == "png")   return "image/png";
        if (ext == "woff2") return "font/woff2";
        if (ext == "json")  return "application/json";
        return "application/octet-stream";
    }
}

PlayerEditor::PlayerEditor (PlayerProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    uiZipStream = std::make_unique<juce::MemoryInputStream> (
        BinaryData::ui_player_zip, BinaryData::ui_player_zipSize, false);
    uiZip = std::make_unique<juce::ZipFile> (uiZipStream.get(), false);

    webView = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options{}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
            .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })
            .withNativeIntegrationEnabled()
            .withOptionsFrom (gainRelay)
            .withOptionsFrom (polyphonyRelay)
            .withOptionsFrom (mpeModeRelay)
            .withOptionsFrom (oversamplingRelay)
            .withOptionsFrom (preloadRelay)
            .withOptionsFrom (sqLiveRelay)
            .withOptionsFrom (sqFreewheelRelay)
            .withOptionsFrom (freewheelRelay)
            .withOptionsFrom (rootKeyRelay)
            .withOptionsFrom (tuningFreqRelay)
            .withOptionsFrom (stretchRelay)
            .withNativeFunction ("loadSfz",       [this] (auto& a, auto c) { handleLoadSfz       (a, std::move (c)); })
            .withNativeFunction ("loadSfzPath",   [this] (auto& a, auto c) { handleLoadSfzPath   (a, std::move (c)); })
            .withNativeFunction ("getStatus",     [this] (auto& a, auto c) { handleGetStatus     (a, std::move (c)); })
            .withNativeFunction ("getAppInfo",    [this] (auto& a, auto c) { handleGetAppInfo    (a, std::move (c)); })
            .withNativeFunction ("getRecentFiles",[this] (auto& a, auto c) { handleGetRecent     (a, std::move (c)); })
            .withNativeFunction ("getCcControls", [this] (auto& a, auto c) { handleGetCcControls (a, std::move (c)); })
            .withNativeFunction ("setCc",         [this] (auto& a, auto c) { handleSetCc         (a, std::move (c)); })
            .withNativeFunction ("getKeyLabels",  [this] (auto& a, auto c) { handleGetKeyLabels  (a, std::move (c)); })
            .withNativeFunction ("noteOn",        [this] (auto& a, auto c) { handleNoteOn        (a, std::move (c)); })
            .withNativeFunction ("noteOff",       [this] (auto& a, auto c) { handleNoteOff       (a, std::move (c)); })
            .withNativeFunction ("loadScala",     [this] (auto& a, auto c) { handleLoadScala     (a, std::move (c)); })
            .withNativeFunction ("resetScala",    [this] (auto& a, auto c) { handleResetScala    (a, std::move (c)); })
            .withNativeFunction ("getTuning",     [this] (auto& a, auto c) { handleGetTuning     (a, std::move (c)); }));

    addAndMakeVisible (*webView);

    auto& apvts = processor.getApvts();
    auto slider = [&apvts] (const char* id, juce::WebSliderRelay& relay)
    {
        return std::make_unique<juce::WebSliderParameterAttachment> (
            *apvts.getParameter (id), relay, nullptr);
    };
    gainAttach        = slider (PlayerEngineParamIds::gainDb,                 gainRelay);
    polyphonyAttach   = slider (PlayerEngineParamIds::polyphony,             polyphonyRelay);
    preloadAttach     = slider (PlayerEngineParamIds::preloadSize,           preloadRelay);
    sqLiveAttach      = slider (PlayerEngineParamIds::sampleQualityLive,     sqLiveRelay);
    sqFreewheelAttach = slider (PlayerEngineParamIds::sampleQualityFreewheel,sqFreewheelRelay);
    rootKeyAttach     = slider (PlayerEngineParamIds::scalaRootKey,          rootKeyRelay);
    tuningFreqAttach  = slider (PlayerEngineParamIds::tuningFrequency,       tuningFreqRelay);
    stretchAttach     = slider (PlayerEngineParamIds::stretchTuning,         stretchRelay);

    mpeModeAttach = std::make_unique<juce::WebComboBoxParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::mpeMode), mpeModeRelay, nullptr);
    oversamplingAttach = std::make_unique<juce::WebComboBoxParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::oversampling), oversamplingRelay, nullptr);
    freewheelAttach = std::make_unique<juce::WebToggleButtonParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::freewheel), freewheelRelay, nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    int width    = kEditorBaseWidth;
    int minWidth = kEditorMinWidth;
#if SAMPLEMACHINE_DEBUG_MIDI_PANEL
    debugPanel = std::make_unique<DebugMidiPanel> (processor.getEngine());
    addAndMakeVisible (*debugPanel);
    width    += kDebugPanelWidth;
    minWidth += kDebugPanelWidth;
#endif

    // The WebView layout is a responsive CSS grid — let the user resize.
    setResizable (true, false);
    setResizeLimits (minWidth, kEditorMinHeight, 2400, 1600);
    setSize (width, kEditorHeight);

    // Seed the CC-control number set so the live reflect-back poll works even
    // before the UI calls getCcControls (e.g. when restoring a session SFZ).
    for (const auto& c : processor.getEngine().getCcControls())
        ccControlNumbers.push_back (c.number);

    startTimerHz (kTelemetryTimerHz);
}

PlayerEditor::~PlayerEditor()
{
    stopTimer();
    releaseHeldNotes();
}

void PlayerEditor::paint (juce::Graphics& g)
{
    // Dark fill so the WebView's initial load doesn't flash white.
    g.fillAll (juce::Colour::fromRGB (0x0a, 0x0b, 0x0e));
}

void PlayerEditor::resized()
{
    auto area = getLocalBounds();

#if SAMPLEMACHINE_DEBUG_MIDI_PANEL
    if (debugPanel != nullptr)
        debugPanel->setBounds (area.removeFromRight (kDebugPanelWidth));
#endif

    if (webView != nullptr)
        webView->setBounds (area);
}

// --- file drag-and-drop (SMPL-89 / SMPL-87) --------------------------------
bool PlayerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".sfz") || f.endsWithIgnoreCase (".scl"))
            return true;
    return false;
}

void PlayerEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase (".sfz"))
        {
            if (processor.loadSfzFile (juce::File (f)))
            {
                emitSfzLoaded();
                return;
            }
        }
        else if (f.endsWithIgnoreCase (".scl"))
        {
            if (processor.loadScalaFile (juce::File (f)) && webView != nullptr)
            {
                webView->emitEventIfBrowserIsVisible ("tuning", makeTuningObject());
                return;
            }
        }
    }
}

PlayerEditor::ResourceResult PlayerEditor::getResource (const juce::String& url)
{
    juce::String path = url.startsWithChar ('/') ? url.substring (1) : url;
    if (path.isEmpty())
        path = "index.html";

    if (uiZip == nullptr)
        return std::nullopt;

    const int index = uiZip->getIndexOfFileName (path);
    if (index < 0)
        return std::nullopt;

    std::unique_ptr<juce::InputStream> entryStream (uiZip->createStreamForEntry (index));
    if (entryStream == nullptr)
        return std::nullopt;

    juce::MemoryBlock block;
    entryStream->readIntoMemoryBlock (block);

    juce::WebBrowserComponent::Resource resource;
    const auto* bytes = static_cast<const std::byte*> (block.getData());
    resource.data.assign (bytes, bytes + block.getSize());
    resource.mimeType = mimeForExtension (path.fromLastOccurrenceOf (".", false, false));
    return resource;
}

void PlayerEditor::timerCallback()
{
    if (webView == nullptr)
        return;

    juce::DynamicObject::Ptr voices = new juce::DynamicObject();
    voices->setProperty ("active", processor.getNumActiveVoices());
    webView->emitEventIfBrowserIsVisible ("voices", juce::var (voices.get()));

    juce::DynamicObject::Ptr meter = new juce::DynamicObject();
    meter->setProperty ("peakL", static_cast<double> (processor.consumePeakL()));
    meter->setProperty ("peakR", static_cast<double> (processor.consumePeakR()));
    webView->emitEventIfBrowserIsVisible ("meter", juce::var (meter.get()));

    // SMPL-88 — keys this UI is holding, for the on-screen keyboard highlight.
    {
        juce::Array<juce::var> on;
        for (int n : heldNotes) on.add (n);
        juce::DynamicObject::Ptr notes = new juce::DynamicObject();
        notes->setProperty ("on", on);
        webView->emitEventIfBrowserIsVisible ("notes", juce::var (notes.get()));
    }

    // SMPL-85 — reflect incoming MIDI / automation CC moves into the knobs
    // (~10 Hz). Poll only the CCs that have controls.
    if (! ccControlNumbers.empty() && (reloadPollCounter % kCcPollEvery) == 0)
    {
        juce::DynamicObject::Ptr cc = new juce::DynamicObject();
        for (int n : ccControlNumbers)
            cc->setProperty (juce::String (n), static_cast<double> (processor.getEngine().getCcValue (n)));
        webView->emitEventIfBrowserIsVisible ("ccValues", juce::var (cc.get()));
    }

    // SMPL-89 / SMPL-87 — on-disk auto-reload (~2 Hz). shouldReload* may open
    // file descriptors, so it is gated to the low sub-rate, message-thread only.
    if (++reloadPollCounter >= kReloadPollEvery)
    {
        reloadPollCounter = 0;
        if (processor.checkForFileReload())
        {
            // Refresh the CC set for the live poll and tell the UI to rebuild.
            ccControlNumbers.clear();
            for (const auto& c : processor.getEngine().getCcControls())
                ccControlNumbers.push_back (c.number);
            webView->emitEventIfBrowserIsVisible ("sfzReloaded", makeStatusObject (true));
            webView->emitEventIfBrowserIsVisible ("sfzLoaded",   makeStatusObject (true));
        }
        if (processor.checkForScalaReload())
            webView->emitEventIfBrowserIsVisible ("tuning", makeTuningObject());
    }
}

juce::var PlayerEditor::makeStatusObject (bool ok) const
{
    const auto file = processor.getCurrentSfzFile();
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("ok", ok);
    obj->setProperty ("sfzPath", file == juce::File() ? juce::String() : file.getFullPathName());
    obj->setProperty ("fileName", file == juce::File() ? juce::String() : file.getFileName());
    obj->setProperty ("numRegions",          processor.getNumRegions());
    obj->setProperty ("numMasters",          processor.getNumMasters());
    obj->setProperty ("numGroups",           processor.getNumGroups());
    obj->setProperty ("numCurves",           processor.getNumCurves());
    obj->setProperty ("numPreloadedSamples", processor.getNumPreloadedSamples());
    obj->setProperty ("numVoices",           processor.getNumVoices());
    obj->setProperty ("sampleRate",          processor.getSampleRate());
    return juce::var (obj.get());
}

juce::var PlayerEditor::makeTuningObject() const
{
    auto& apvts = processor.getApvts();
    auto val = [&apvts] (const char* id) -> double
    {
        if (auto* r = apvts.getRawParameterValue (id)) return static_cast<double> (r->load());
        return 0.0;
    };
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("scaleName", processor.getScalaName());
    obj->setProperty ("rootKey",   val (PlayerEngineParamIds::scalaRootKey));
    obj->setProperty ("tuningHz",  val (PlayerEngineParamIds::tuningFrequency));
    obj->setProperty ("stretch",   val (PlayerEngineParamIds::stretchTuning));
    return juce::var (obj.get());
}

void PlayerEditor::emitSfzLoaded()
{
    ccControlNumbers.clear();
    for (const auto& c : processor.getEngine().getCcControls())
        ccControlNumbers.push_back (c.number);
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible ("sfzLoaded", makeStatusObject (true));
}

void PlayerEditor::releaseHeldNotes()
{
    for (int n : heldNotes)
        processor.injectNote (false, n, 0);
    heldNotes.clear();
}

// --- native function handlers ----------------------------------------------
void PlayerEditor::handleGetStatus (const juce::Array<juce::var>&, Completion completion)
{
    completion (makeStatusObject (true));
}

void PlayerEditor::handleGetAppInfo (const juce::Array<juce::var>&, Completion completion)
{
    // Single source of truth for the wordmark + version: the JUCE plugin
    // defines (CMake PRODUCT_NAME / VERSION). The UI no longer hardcodes them.
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("productName", juce::String (JucePlugin_Name));
    obj->setProperty ("version",     juce::String (JucePlugin_VersionString));
    completion (juce::var (obj.get()));
}

void PlayerEditor::handleLoadSfz (const juce::Array<juce::var>&, Completion completion)
{
    auto initial = processor.getCurrentSfzFile();
    if (! initial.existsAsFile())
        initial = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load SFZ instrument", initial, "*.sfz");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    auto shared = std::make_shared<Completion> (std::move (completion));
    chooser->launchAsync (flags, [this, shared] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
        {
            (*shared) (makeStatusObject (false));
            return;
        }
        const bool ok = processor.loadSfzFile (file);
        if (ok) emitSfzLoaded();
        (*shared) (makeStatusObject (ok));
    });
}

void PlayerEditor::handleLoadSfzPath (const juce::Array<juce::var>& args, Completion completion)
{
    if (args.isEmpty())
    {
        completion (makeStatusObject (false));
        return;
    }
    const juce::File file (args[0].toString());
    const bool ok = file.existsAsFile() && processor.loadSfzFile (file);
    if (ok) emitSfzLoaded();
    completion (makeStatusObject (ok));
}

void PlayerEditor::handleGetRecent (const juce::Array<juce::var>&, Completion completion)
{
    juce::Array<juce::var> out;
    for (const auto& path : processor.getRecentFiles())
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("path", path);
        o->setProperty ("fileName", juce::File (path).getFileName());
        out.add (juce::var (o.get()));
    }
    completion (juce::var (out));
}

void PlayerEditor::handleGetCcControls (const juce::Array<juce::var>&, Completion completion)
{
    ccControlNumbers.clear();
    juce::Array<juce::var> out;
    for (const auto& c : processor.getEngine().getCcControls())
    {
        ccControlNumbers.push_back (c.number);
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("number",       c.number);
        o->setProperty ("label",        c.label);
        o->setProperty ("value",        static_cast<double> (processor.getEngine().getCcValue (c.number)));
        o->setProperty ("defaultValue", static_cast<double> (c.defaultValue));
        o->setProperty ("isSwitch",     c.isSwitch);
        out.add (juce::var (o.get()));
    }
    completion (juce::var (out));
}

void PlayerEditor::handleSetCc (const juce::Array<juce::var>& args, Completion completion)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    if (args.size() >= 2)
    {
        processor.getEngine().setCcValue (static_cast<int> (args[0]), static_cast<float> (static_cast<double> (args[1])));
        o->setProperty ("ok", true);
    }
    else
    {
        o->setProperty ("ok", false);
    }
    completion (juce::var (o.get()));
}

void PlayerEditor::handleGetKeyLabels (const juce::Array<juce::var>&, Completion completion)
{
    juce::Array<juce::var> labels, used;
    for (const auto& kv : processor.getEngine().getKeyLabels())
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("key", kv.first);
        o->setProperty ("text", kv.second);
        labels.add (juce::var (o.get()));
        used.add (kv.first);
    }
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("labels", labels);
    obj->setProperty ("usedKeys", used);
    completion (juce::var (obj.get()));
}

void PlayerEditor::handleNoteOn (const juce::Array<juce::var>& args, Completion completion)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    if (args.size() >= 2)
    {
        const int note = static_cast<int> (args[0]);
        const int vel  = juce::jlimit (1, 127, static_cast<int> (args[1]));
        processor.injectNote (true, note, vel);
        heldNotes.insert (note);
        o->setProperty ("ok", true);
    }
    else o->setProperty ("ok", false);
    completion (juce::var (o.get()));
}

void PlayerEditor::handleNoteOff (const juce::Array<juce::var>& args, Completion completion)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    if (! args.isEmpty())
    {
        const int note = static_cast<int> (args[0]);
        processor.injectNote (false, note, 0);
        heldNotes.erase (note);
        o->setProperty ("ok", true);
    }
    else o->setProperty ("ok", false);
    completion (juce::var (o.get()));
}

void PlayerEditor::handleLoadScala (const juce::Array<juce::var>&, Completion completion)
{
    chooser = std::make_unique<juce::FileChooser> ("Load Scala scale",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.scl");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    auto shared = std::make_shared<Completion> (std::move (completion));
    chooser->launchAsync (flags, [this, shared] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file != juce::File())
            processor.loadScalaFile (file);
        (*shared) (makeTuningObject());
    });
}

void PlayerEditor::handleResetScala (const juce::Array<juce::var>&, Completion completion)
{
    processor.resetScala();
    completion (makeTuningObject());
}

void PlayerEditor::handleGetTuning (const juce::Array<juce::var>&, Completion completion)
{
    completion (makeTuningObject());
}

} // namespace samplemachine
