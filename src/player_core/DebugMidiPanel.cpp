#include "DebugMidiPanel.h"

#include <PlayerEngine.h>

namespace samplemachine
{

DebugMidiPanel::DebugMidiPanel (PlayerEngine& e)
    : engine (e)
{
    log.setMultiLine (true);
    log.setReadOnly (true);
    log.setScrollbarsShown (true);
    log.setCaretVisible (false);
    log.setFont (juce::FontOptions ("Menlo", 11.0f, juce::Font::plain));
    log.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (16, 16, 18));
    log.setColour (juce::TextEditor::textColourId,       juce::Colour::fromRGB (210, 210, 210));
    log.setColour (juce::TextEditor::outlineColourId,    juce::Colour::fromRGB (40, 40, 40));
    addAndMakeVisible (log);

    clearButton.onClick = [this]
    {
        log.clear();
        haveOrigin = false;
    };
    addAndMakeVisible (clearButton);

    anchorButton.setClickingTogglesState (true);
    anchorButton.setToggleState (anchorOnNoteOn, juce::dontSendNotification);
    anchorButton.onClick = [this]
    {
        anchorOnNoteOn = anchorButton.getToggleState();
        haveOrigin = false;
    };
    addAndMakeVisible (anchorButton);

    startTimerHz (15);
}

DebugMidiPanel::~DebugMidiPanel() = default;

void DebugMidiPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (24, 24, 26));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawFittedText ("MIDI debug",
                       getLocalBounds().reduced (8).withHeight (20),
                       juce::Justification::centredLeft, 1);
}

void DebugMidiPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (24); // header

    auto controls = area.removeFromTop (28);
    clearButton.setBounds  (controls.removeFromLeft (60));
    controls.removeFromLeft (8);
    anchorButton.setBounds (controls.removeFromLeft (140));

    area.removeFromTop (4);
    log.setBounds (area);
}

juce::String DebugMidiPanel::formatEvent (const DebugMidiCapture::Event& e,
                                            double sampleRate,
                                            std::int64_t origin) const
{
    const double ms = DebugMidiCapture::samplesToMs (e.timestampSamples - origin, sampleRate);

    auto fmtMs = [ms]
    {
        return juce::String (ms, 1).paddedLeft (' ', 8);
    };

    auto chStr = juce::String ((int) e.channel + 1).paddedLeft (' ', 2);

    switch (e.type)
    {
        case DebugMidiCapture::EventType::NoteOn:
            return fmtMs() + " ms  ch" + chStr + "  noteOn  note=" + juce::String (e.data1) + " vel=" + juce::String (e.data2);
        case DebugMidiCapture::EventType::NoteOff:
            return fmtMs() + " ms  ch" + chStr + "  noteOff note=" + juce::String (e.data1) + " vel=" + juce::String (e.data2);
        case DebugMidiCapture::EventType::CC:
            return fmtMs() + " ms  ch" + chStr + "  CC      num=" + juce::String (e.data1).paddedLeft (' ', 3)
                   + " val=" + juce::String (e.data2);
        case DebugMidiCapture::EventType::PitchBend:
            return fmtMs() + " ms  ch" + chStr + "  PitchBd       val=" + juce::String (e.data1);
        case DebugMidiCapture::EventType::ChannelPressure:
            return fmtMs() + " ms  ch" + chStr + "  ChPress       val=" + juce::String (e.data1);
        case DebugMidiCapture::EventType::PolyAftertouch:
            return fmtMs() + " ms  ch" + chStr + "  PolyAT  note=" + juce::String (e.data1) + " val=" + juce::String (e.data2);
    }
    return {};
}

void DebugMidiPanel::timerCallback()
{
    auto& capture = engine.getDebugMidiCapture();
    const int got = capture.drain (scratch, kBufferSize);
    if (got <= 0) return;

    const double sr = engine.getDebugSampleRate();

    juce::String batch;
    for (int i = 0; i < got; ++i)
    {
        const auto& e = scratch[i];
        if (! haveOrigin)
        {
            const bool anchorReady = anchorOnNoteOn
                ? (e.type == DebugMidiCapture::EventType::NoteOn)
                : true;
            if (! anchorReady) continue;
            originSamples = e.timestampSamples;
            haveOrigin = true;
        }
        batch << formatEvent (e, sr, originSamples) << '\n';
    }

    if (batch.isNotEmpty())
    {
        log.moveCaretToEnd();
        log.insertTextAtCaret (batch);

        // Cap log to last ~5000 lines so it doesn't grow unbounded.
        const auto text = log.getText();
        constexpr int kMaxChars = 200000;
        if (text.length() > kMaxChars)
        {
            log.setText (text.substring (text.length() - kMaxChars), false);
            log.moveCaretToEnd();
        }
    }
}

} // namespace samplemachine
