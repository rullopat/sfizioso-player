#pragma once

#include <DebugMidiCapture.h>

#include <juce_gui_basics/juce_gui_basics.h>

namespace samplemachine
{

class PlayerEngine;

/**
 * @brief Right-side debug pane showing every MIDI event the engine
 * receives in realtime, with timestamps relative to the first event of
 * the currently displayed window. Used to verify whether the host is
 * delivering per-channel MPE events in the order the recorded MIDI
 * region claims (suspected re-ordering on Logic Pro).
 *
 * Compiled in only when the SAMPLEMACHINE_DEBUG_MIDI_PANEL CMake option
 * is on (default ON for Debug builds, OFF for Release). The player and
 * rompler editors both render it on the right side of their windows
 * when the symbol is defined.
 */
class DebugMidiPanel : public juce::Component,
                       public juce::Timer
{
public:
    explicit DebugMidiPanel (PlayerEngine& engine);
    ~DebugMidiPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String formatEvent (const DebugMidiCapture::Event& e, double sampleRate, std::int64_t originSamples) const;

    PlayerEngine&    engine;
    juce::TextEditor log;
    juce::TextButton clearButton { "Clear" };
    juce::TextButton anchorButton { "Anchor on noteOn" };

    bool        anchorOnNoteOn = true;
    std::int64_t originSamples = 0;
    bool        haveOrigin = false;

    static constexpr int kBufferSize = 2048;
    DebugMidiCapture::Event scratch[kBufferSize] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugMidiPanel)
};

} // namespace samplemachine
