#include "PlayerEditor.h"
#include "PlayerProcessor.h"

#include "BinaryData.h"

namespace samplemachine
{

namespace
{
    constexpr int kEditorBaseWidth  = 720;
    constexpr int kEditorHeight     = 480;
    constexpr int kDebugPanelWidth  = 360;
    // 30 Hz keeps the meter responsive without flooding the message thread;
    // the active-voices readout is fine at the same rate.
    constexpr int kTelemetryTimerHz = 30;

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
            .withNativeFunction ("loadSfz",
                [this] (const juce::Array<juce::var>& args, auto completion)
                { handleLoadSfz (args, std::move (completion)); })
            .withNativeFunction ("getStatus",
                [this] (const juce::Array<juce::var>& args, auto completion)
                { handleGetStatus (args, std::move (completion)); }));

    addAndMakeVisible (*webView);

    auto& apvts = processor.getApvts();
    gainAttach = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::gainDb), gainRelay, nullptr);
    polyphonyAttach = std::make_unique<juce::WebSliderParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::polyphony), polyphonyRelay, nullptr);
    mpeModeAttach = std::make_unique<juce::WebComboBoxParameterAttachment> (
        *apvts.getParameter (PlayerEngineParamIds::mpeMode), mpeModeRelay, nullptr);

    webView->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    int width = kEditorBaseWidth;
#if SAMPLEMACHINE_DEBUG_MIDI_PANEL
    debugPanel = std::make_unique<DebugMidiPanel> (processor.getEngine());
    addAndMakeVisible (*debugPanel);
    width += kDebugPanelWidth;
#endif
    setSize (width, kEditorHeight);

    startTimerHz (kTelemetryTimerHz);
}

PlayerEditor::~PlayerEditor() = default;

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
}

juce::var PlayerEditor::makeStatusObject (bool ok) const
{
    const auto file = processor.getCurrentSfzFile();
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("ok", ok);
    obj->setProperty ("sfzPath", file == juce::File() ? juce::String() : file.getFullPathName());
    obj->setProperty ("fileName", file == juce::File() ? juce::String() : file.getFileName());
    obj->setProperty ("numRegions", processor.getNumRegions());
    obj->setProperty ("numPreloadedSamples", processor.getNumPreloadedSamples());
    return juce::var (obj.get());
}

void PlayerEditor::handleGetStatus (const juce::Array<juce::var>&,
                                    juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    completion (makeStatusObject (true));
}

void PlayerEditor::handleLoadSfz (const juce::Array<juce::var>&,
                                  juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    auto initial = processor.getCurrentSfzFile();
    if (! initial.existsAsFile())
        initial = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load SFZ instrument", initial, "*.sfz");
    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    auto sharedCompletion = std::make_shared<juce::WebBrowserComponent::NativeFunctionCompletion> (
        std::move (completion));

    chooser->launchAsync (flags, [this, sharedCompletion] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
        {
            (*sharedCompletion) (makeStatusObject (false));
            return;
        }

        const bool ok = processor.loadSfzFile (file);
        (*sharedCompletion) (makeStatusObject (ok));
    });
}

} // namespace samplemachine
