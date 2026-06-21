#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sfz { class SampleReader; }

namespace samplemachine
{

class PortableBundle
{
public:
    PortableBundle();
    ~PortableBundle();

    bool loadFromFile (const juce::File& file);

    bool isValid() const noexcept { return valid; }
    const juce::String& getError() const noexcept { return errorMessage; }
    const juce::String& getSfzText() const noexcept { return sfzText; }
    juce::String getVirtualPath() const;
    sfz::SampleReader* getSampleReader() noexcept { return sampleReader.get(); }

    const juce::File& getFile() const noexcept { return sourceFile; }
    const juce::String& getInstrumentName() const noexcept { return instrumentName; }
    const juce::String& getPatchName() const noexcept { return patchName; }

    struct SampleBytes
    {
        const void* data = nullptr;
        std::size_t size = 0;
    };

    SampleBytes readSampleBytes (const std::string& path) const;

private:
    bool parseBytes (const std::uint8_t* bytes, std::size_t size);

    bool valid = false;
    juce::String errorMessage;
    juce::File sourceFile;
    std::vector<std::uint8_t> bundleBytes;
    juce::String sfzText;
    juce::String instrumentName;
    juce::String patchName;
    std::unordered_map<std::string, SampleBytes> samples;
    std::unique_ptr<sfz::SampleReader> sampleReader;
};

} // namespace samplemachine
