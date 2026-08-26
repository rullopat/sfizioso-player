#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace samplemachine
{

/**
 * @brief Lock-free ring buffer of recent MIDI events as they arrive at
 * PlayerEngine::processBlock, for in-plugin debug display. Producer is
 * the audio thread, consumer is the message thread (Timer-driven UI).
 *
 * Capacity is fixed; if the UI doesn't drain fast enough older events
 * are silently overwritten — fine for a real-time monitor.
 */
class DebugMidiCapture
{
public:
    enum class EventType : std::uint8_t
    {
        NoteOn,
        NoteOff,
        CC,
        PitchBend,
        ChannelPressure,
        PolyAftertouch
    };

    struct Event
    {
        std::int64_t timestampSamples; // sample-count clock since plugin construction
        EventType    type;
        std::uint8_t channel;          // sfizz 0..15
        std::int32_t data1;            // note / cc number; pitch-bend signed 14-bit value
        std::int32_t data2;            // velocity / cc value / aftertouch
    };

    static constexpr int kCapacity = 1024;

    /** Audio-thread side: push a single event. RT-safe. */
    void push (const Event& e) noexcept
    {
        const auto writePos = writeIdx_.load (std::memory_order_relaxed);
        buffer_[static_cast<std::size_t> (writePos & (kCapacity - 1))] = e;
        writeIdx_.store (writePos + 1, std::memory_order_release);
    }

    /**
     * @brief Drain events newer than the last call. Returns the number
     * of events copied. If the producer wrote more than kCapacity since
     * the last drain, the oldest are silently dropped.
     */
    int drain (Event* dst, int dstCapacity) noexcept
    {
        const auto write = writeIdx_.load (std::memory_order_acquire);
        auto read = readIdx_;
        const auto available = static_cast<int> (write - read);
        const auto behind = available - kCapacity;
        if (behind > 0)
            read += behind; // dropped some, jump forward to the oldest still in buffer

        const auto toCopy = juce::jmin (static_cast<int> (write - read), dstCapacity);
        for (int i = 0; i < toCopy; ++i)
            dst[i] = buffer_[static_cast<std::size_t> ((read + i) & (kCapacity - 1))];

        readIdx_ = read + static_cast<std::int64_t> (toCopy);
        return toCopy;
    }

    /** UI-side helper. Translates timestamp into milliseconds since plugin start. */
    static double samplesToMs (std::int64_t samples, double sampleRate) noexcept
    {
        if (sampleRate <= 0.0) return 0.0;
        return 1000.0 * static_cast<double> (samples) / sampleRate;
    }

private:
    static_assert ((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of two");

    std::array<Event, kCapacity> buffer_ {};
    std::atomic<std::int64_t>    writeIdx_ { 0 }; // monotonically increasing
    std::int64_t                 readIdx_  { 0 };
};

} // namespace samplemachine
