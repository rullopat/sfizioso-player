#include "PortableBundle.h"

#include <sfizioso.hpp>
#include <sfzbundle/Parser.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace samplemachine
{
namespace
{

class PortableBundleSampleReader final : public sfizioso::SampleReader
{
public:
    explicit PortableBundleSampleReader (const PortableBundle& bundleIn)
        : bundle (bundleIn)
    {
    }

    sfizioso::SampleData read (const std::string& path) override
    {
        const auto bytes = bundle.readSampleBytes (path);
        return { bytes.data, bytes.size };
    }

private:
    const PortableBundle& bundle;
};

std::string normaliseKey (std::string path)
{
    std::replace (path.begin(), path.end(), '\\', '/');
    return path;
}

std::string basenameKey (const std::string& path)
{
    const auto slash = path.find_last_of ('/');
    return slash == std::string::npos ? path : path.substr (slash + 1);
}

bool readUtf8 (sfzbundle::ByteView bytes, juce::String& result)
{
    if (bytes.size > static_cast<std::size_t> (std::numeric_limits<int>::max()))
        return false;

    if (! sfzbundle::isValidUtf8 (bytes))
        return false;

    const auto length = static_cast<int> (bytes.size);
    result = juce::String::fromUTF8 (
        reinterpret_cast<const char*> (bytes.data), length);
    return true;
}

} // namespace

PortableBundle::PortableBundle()
{
    sampleReader = std::make_unique<PortableBundleSampleReader> (*this);
}

PortableBundle::~PortableBundle() = default;

bool PortableBundle::loadFromFile (const juce::File& file)
{
    valid = false;
    errorMessage.clear();
    sourceFile = file;
    samples.clear();
    bundleBytes.clear();
    sfzText.clear();
    instrumentName.clear();
    patchName.clear();

    if (! file.existsAsFile())
    {
        errorMessage = "Bundle file not found: " + file.getFullPathName();
        return false;
    }

    juce::MemoryBlock bytes;
    if (! file.loadFileAsData (bytes))
    {
        errorMessage = "Cannot read bundle file: " + file.getFullPathName();
        return false;
    }

    if (bytes.isEmpty())
    {
        errorMessage = "Invalid .sfzbundle header";
        return false;
    }

    const auto* first = static_cast<const std::uint8_t*> (bytes.getData());
    bundleBytes.assign (first, first + bytes.getSize());
    valid = parseBytes (bundleBytes.data(), bundleBytes.size());
    return valid;
}

juce::String PortableBundle::getVirtualPath() const
{
    return "/__sfizioso_bundle__/" + sourceFile.getFileNameWithoutExtension() + ".sfz";
}

PortableBundle::SampleBytes PortableBundle::readSampleBytes (const std::string& path) const
{
    const auto key = normaliseKey (path);
    auto it = samples.find (key);
    if (it != samples.end())
        return it->second;

    it = samples.find (basenameKey (key));
    if (it != samples.end())
        return it->second;

    return {};
}

bool PortableBundle::parseBytes (const std::uint8_t* bytes, std::size_t size)
{
    sfzbundle::HeaderView header;
    sfzbundle::ParseError headerError;
    if (! sfzbundle::inspectHeader (bytes, size, header, headerError))
    {
        errorMessage = "Invalid .sfzbundle header";
        return false;
    }

    if ((header.flags & sfzbundle::kBundleFlagEncrypted) != 0)
    {
        errorMessage = "Encrypted .sfzbundle files are not supported by this player";
        return false;
    }

    sfzbundle::BundleView bundle;
    if (! bundle.parse (bytes, size))
    {
        const auto& parseError = bundle.error();
        if (parseError.entryIndex != sfzbundle::ParseError::noEntry)
            errorMessage = "Invalid bundle entry at index "
                           + juce::String (parseError.entryIndex);
        else
            errorMessage = "Invalid .sfzbundle header";
        return false;
    }

    // Build a complete temporary view first. A semantic failure must not expose
    // pointers from an otherwise-invalid bundle through readSampleBytes.
    std::unordered_map<std::string, SampleBytes> parsedSamples;
    juce::String parsedSfzText;
    juce::String metadataJson;
    bool hasMetadata = false;

    for (std::size_t i = 0; i < bundle.entries().size(); ++i)
    {
        const auto& entry = bundle.entries()[i];
        if (entry.type == sfzbundle::BundleEntryType::SampleWav
            || entry.type == sfzbundle::BundleEntryType::SampleFlac)
        {
            const auto key = normaliseKey (std::string (entry.name));
            if (key.empty())
            {
                errorMessage = "Bundle sample entry has an empty name at index " + juce::String (i);
                return false;
            }

            const SampleBytes sample { entry.payload.data, entry.payload.size };
            parsedSamples[key] = sample;
            parsedSamples[basenameKey (key)] = sample;
        }
        else if (entry.type == sfzbundle::BundleEntryType::MetadataJson)
        {
            hasMetadata = true;
            if (! readUtf8 (entry.payload, metadataJson))
            {
                errorMessage = "Bundle metadata is not valid UTF-8";
                return false;
            }
        }
        else if (entry.type == sfzbundle::BundleEntryType::SfzText
                 && parsedSfzText.isEmpty())
        {
            if (! readUtf8 (entry.payload, parsedSfzText))
            {
                errorMessage = "Bundle SFZ preset is not valid UTF-8";
                return false;
            }
        }
    }

    if (parsedSfzText.isEmpty())
    {
        errorMessage = "Bundle contains no SFZ preset";
        return false;
    }

    juce::String parsedInstrumentName;
    juce::String parsedPatchName;
    if (hasMetadata)
    {
        juce::var parsedMetadata;
        const auto result = juce::JSON::parse (metadataJson, parsedMetadata);
        auto* obj = parsedMetadata.getDynamicObject();
        if (result.failed() || obj == nullptr)
        {
            errorMessage = "Bundle metadata is not a valid JSON object";
            return false;
        }

        parsedInstrumentName = obj->getProperty ("instrumentName").toString();
        parsedPatchName = obj->getProperty ("patchName").toString();
    }

    samples = std::move (parsedSamples);
    sfzText = std::move (parsedSfzText);
    instrumentName = std::move (parsedInstrumentName);
    patchName = std::move (parsedPatchName);
    return true;
}

} // namespace samplemachine
