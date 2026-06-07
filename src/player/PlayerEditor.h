#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <set>

namespace samplemachine
{

class PlayerProcessor;

/**
 * SMPL-56 — WebView-based player editor.
 *
 * Hosts a juce::WebBrowserComponent that loads a React + TypeScript app from
 * a single-file Vite bundle embedded via juce_add_binary_data. Parameters are
 * exposed through WebSliderRelay / WebComboBoxRelay / WebToggleButtonRelay;
 * the file chooser, introspection queries, and the live streams cross the
 * bridge via native functions and timer-batched events.
 *
 * Member declaration order is load-bearing — see AGENTS.md "Critical rules"
 * for the relays → webview → attachments rule.
 */
class PlayerEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit PlayerEditor (PlayerProcessor&);
    ~PlayerEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    // SMPL-89 / SMPL-87 — native file drop (the WKWebView passes OS file drags
    // through to the parent target; .sfz loads the instrument, .scl the scale).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using ResourceResult = std::optional<juce::WebBrowserComponent::Resource>;
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    ResourceResult getResource (const juce::String& url);
    void timerCallback() override;

    // --- native function handlers ------------------------------------------
    void handleLoadSfz      (const juce::Array<juce::var>& args, Completion);
    void handleLoadSfzPath  (const juce::Array<juce::var>& args, Completion); // SMPL-89
    void handleGetStatus    (const juce::Array<juce::var>& args, Completion);
    void handleGetAppInfo   (const juce::Array<juce::var>& args, Completion);
    void handleGetRecent    (const juce::Array<juce::var>& args, Completion); // SMPL-89
    void handleGetCcControls(const juce::Array<juce::var>& args, Completion); // SMPL-85
    void handleSetCc        (const juce::Array<juce::var>& args, Completion); // SMPL-85
    void handleGetKeyLabels (const juce::Array<juce::var>& args, Completion); // SMPL-88
    void handleNoteOn       (const juce::Array<juce::var>& args, Completion); // SMPL-88
    void handleNoteOff      (const juce::Array<juce::var>& args, Completion); // SMPL-88
    void handleLoadScala    (const juce::Array<juce::var>& args, Completion); // SMPL-87
    void handleResetScala   (const juce::Array<juce::var>& args, Completion); // SMPL-87
    void handleGetTuning    (const juce::Array<juce::var>& args, Completion); // SMPL-87

    juce::var makeStatusObject (bool ok) const;
    juce::var makeTuningObject() const;
    void emitSfzLoaded();
    void releaseHeldNotes();

    PlayerProcessor& processor;

    // Zip backing the bundled UI; stream owns the bytes, zip reads from it.
    std::unique_ptr<juce::MemoryInputStream> uiZipStream;
    std::unique_ptr<juce::ZipFile> uiZip;

    // CC numbers with auto-generated controls — polled for live reflect-back.
    std::vector<int> ccControlNumbers;
    // Keys this UI is currently holding (for the keyboard's lit-key highlight).
    std::set<int> heldNotes;
    int reloadPollCounter = 0;

    // -----------------------------------------------------------------------
    // CRITICAL: destruction order is reverse of declaration.
    // The WebView holds references to relays via lambdas (Options{}.withOptionsFrom).
    // Attachments hold references to the WebView's internal state.
    // Order MUST be: relays first → webview → attachments last.
    // See AGENTS.md "UI: WebView + React" — wrong order = crash on editor close.
    // -----------------------------------------------------------------------

    juce::WebSliderRelay        gainRelay        { "gainDb" };
    juce::WebSliderRelay        polyphonyRelay   { "polyphony" };
    juce::WebComboBoxRelay      mpeModeRelay      { "mpeMode" };
    juce::WebComboBoxRelay      oversamplingRelay { "oversampling" };
    juce::WebComboBoxRelay      preloadRelay      { "preloadSize" };
    juce::WebComboBoxRelay      sqLiveRelay       { "sampleQualityLive" };
    juce::WebComboBoxRelay      sqFreewheelRelay  { "sampleQualityFreewheel" };
    juce::WebComboBoxRelay      oscQualityLiveRelay     { "oscQualityLive" };
    juce::WebComboBoxRelay      oscQualityFreewheelRelay { "oscQualityFreewheel" };
    juce::WebToggleButtonRelay  freewheelRelay    { "freewheel" };
    juce::WebToggleButtonRelay  sustainRelay      { "sustainCancelsRelease" };
    juce::WebSliderRelay        rootKeyRelay      { "scalaRootKey" };
    juce::WebSliderRelay        tuningFreqRelay   { "tuningFrequency" };
    juce::WebSliderRelay        stretchRelay      { "stretchTuning" };
    juce::WebSliderRelay        mpeMasterRelay    { "mpeMasterBend" };
    juce::WebSliderRelay        mpePerNoteRelay   { "mpePerNoteBend" };
    juce::WebToggleButtonRelay  mpeIgnoreMasterRelay  { "mpeIgnoreMasterRpn" };
    juce::WebToggleButtonRelay  mpeIgnorePerNoteRelay { "mpeIgnorePerNoteRpn" };

    std::unique_ptr<juce::WebBrowserComponent> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment>       gainAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       polyphonyAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     mpeModeAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     oversamplingAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     preloadAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     sqLiveAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     sqFreewheelAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     oscQualityLiveAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     oscQualityFreewheelAttach;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> freewheelAttach;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> sustainAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       rootKeyAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       tuningFreqAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       stretchAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       mpeMasterAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>       mpePerNoteAttach;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> mpeIgnoreMasterAttach;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> mpeIgnorePerNoteAttach;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
};

} // namespace samplemachine
