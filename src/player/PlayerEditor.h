#pragma once

#if SAMPLEMACHINE_DEBUG_MIDI_PANEL
 #include <DebugMidiPanel.h>
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace samplemachine
{

class PlayerProcessor;

/**
 * SMPL-56 — WebView-based player editor.
 *
 * Hosts a juce::WebBrowserComponent that loads a React + TypeScript app from
 * a single-file Vite bundle embedded via juce_add_binary_data. Parameters are
 * exposed through WebSliderRelay / WebComboBoxRelay; the file chooser, the
 * status query, and the active-voices stream cross the bridge via native
 * functions and timer-batched events.
 *
 * Member declaration order is load-bearing — see CLAUDE.md "Critical rules"
 * for the relays → webview → attachments rule.
 */
class PlayerEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PlayerEditor (PlayerProcessor&);
    ~PlayerEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    using ResourceResult = std::optional<juce::WebBrowserComponent::Resource>;

    ResourceResult getResource (const juce::String& url);
    void timerCallback() override;

    void handleLoadSfz    (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion);
    void handleGetStatus  (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion);
    void handleGetAppInfo (const juce::Array<juce::var>& args,
                           juce::WebBrowserComponent::NativeFunctionCompletion completion);

    juce::var makeStatusObject (bool ok) const;

    PlayerProcessor& processor;

    // Zip backing the bundled UI; stream owns the bytes, zip reads from it.
    std::unique_ptr<juce::MemoryInputStream> uiZipStream;
    std::unique_ptr<juce::ZipFile> uiZip;

    // -----------------------------------------------------------------------
    // CRITICAL: destruction order is reverse of declaration.
    // The WebView holds references to relays via lambdas (Options{}.withOptionsFrom).
    // Attachments hold references to the WebView's internal state.
    // Order MUST be: relays first → webview → attachments last.
    // See CLAUDE.md "UI: WebView + React (in flight)" — wrong order = crash on
    // editor close.
    // -----------------------------------------------------------------------

    juce::WebSliderRelay   gainRelay      { "gainDb" };
    juce::WebSliderRelay   polyphonyRelay { "polyphony" };
    juce::WebComboBoxRelay mpeModeRelay   { "mpeMode" };

    std::unique_ptr<juce::WebBrowserComponent> webView;

    std::unique_ptr<juce::WebSliderParameterAttachment>   gainAttach;
    std::unique_ptr<juce::WebSliderParameterAttachment>   polyphonyAttach;
    std::unique_ptr<juce::WebComboBoxParameterAttachment> mpeModeAttach;

    std::unique_ptr<juce::FileChooser> chooser;

#if SAMPLEMACHINE_DEBUG_MIDI_PANEL
    std::unique_ptr<DebugMidiPanel> debugPanel;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerEditor)
};

} // namespace samplemachine
