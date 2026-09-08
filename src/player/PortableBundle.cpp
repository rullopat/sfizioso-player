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
    assets.clear();
    manifest = {};
    sfzEntryName.clear();
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
    return "/__sfizioso_bundle__/" + sfzEntryName;
}

PortableBundle::SampleBytes PortableBundle::readSampleBytes (const std::string& path) const
{
    auto key = normaliseKey (path);
    const std::string prefix = "/__sfizioso_bundle__/";
    if (key.compare (0, prefix.size(), prefix) == 0) key.erase (0, prefix.size());
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
    std::vector<const sfzbundle::EntryView*> sfzs;
    const sfzbundle::EntryView* canonical = nullptr;
    const sfzbundle::EntryView* legacy = nullptr;
    bool duplicateManifest = false;
    for (const auto& entry : bundle.entries())
    {
        if (entry.type == sfzbundle::BundleEntryType::SampleWav
            || entry.type == sfzbundle::BundleEntryType::SampleFlac)
        {
            const auto key = normaliseKey (std::string (entry.name));
            if (key.empty()) { errorMessage = "Bundle sample entry has an empty name"; return false; }
            const SampleBytes sample { entry.payload.data, entry.payload.size };
            parsedSamples[key] = sample;
            parsedSamples[basenameKey (key)] = sample;
        }
        else if (entry.type == sfzbundle::BundleEntryType::SfzText)
            sfzs.push_back (&entry);
        else if (entry.type == sfzbundle::BundleEntryType::MetadataJson)
        {
            if (entry.name == "instrument.json")
            {
                duplicateManifest = canonical != nullptr;
                canonical = &entry;
            }
            else if (entry.name == "presets.json") legacy = &entry;
        }
        else if (entry.type == sfzbundle::BundleEntryType::Asset)
        {
            const auto name = juce::String::fromUTF8 (entry.name.data(), static_cast<int> (entry.name.size()));
            if (sfizioso_manifest::isPackagePath (name) && entry.payload.size <= sfizioso_manifest::maxAssetBytes)
            {
                const auto inserted = assets.emplace (std::string (entry.name), SampleBytes { entry.payload.data, entry.payload.size });
                if (! inserted.second) inserted.first->second = {}; // ambiguous asset names are unusable
            }
        }
    }
    if (sfzs.empty()) { errorMessage = "Bundle contains no SFZ preset"; return false; }
    auto* selected = sfzs.front();
    if (canonical && ! duplicateManifest)
        manifest = sfizioso_manifest::parse (canonical->payload.data, canonical->payload.size);
    else if (duplicateManifest)
        manifest.diagnostics.add ("Ignored duplicate instrument.json entries.");
    if (manifest.manifest)
    {
        // Resolve every catalogue entry exactly. Metadata failure never prevents
        // loading the original first SFZ entry.
        for (const auto& preset : manifest.manifest->presets)
        {
            int count = 0;
            for (const auto* entry : sfzs)
                if (entry->name == preset.sfz.toStdString()) ++count;
            if (count != 1)
            {
                manifest.manifest.reset();
                manifest.diagnostics.add ("Manifest names a missing or ambiguous SFZ entry.");
                break;
            }
        }
    }
    juce::String parsedInstrumentName, parsedPatchName;
    if (manifest.manifest)
    {
        const auto& preset = manifest.manifest->presets.front();
        for (const auto* entry : sfzs)
            if (entry->name == preset.sfz.toStdString()) selected = entry;
        parsedInstrumentName = manifest.manifest->name;
        parsedPatchName = preset.name;
    }
    else if (legacy && legacy->payload.size <= sfizioso_manifest::maxJsonBytes)
    {
        // Legacy metadata is optional as well. Bound nesting before JUCE JSON
        // parsing through the shared parser's preflight (legacy adaptation below).
        const auto adapted = sfizioso_manifest::parseLegacy (legacy->payload.data, legacy->payload.size);
        parsedInstrumentName = adapted.first;
        parsedPatchName = adapted.second;
    }
    juce::String parsedSfzText;
    if (! readUtf8 (selected->payload, parsedSfzText))
    { errorMessage = "Bundle SFZ preset is not valid UTF-8"; return false; }
    if (parsedSfzText.isEmpty()) { errorMessage = "Bundle contains no SFZ preset"; return false; }
    sfzEntryName = juce::String::fromUTF8 (selected->name.data(), static_cast<int> (selected->name.size()));
    samples = std::move (parsedSamples);
    sfzText = std::move (parsedSfzText);
    instrumentName = std::move (parsedInstrumentName);
    patchName = std::move (parsedPatchName);
    return true;
}

PortableBundle::SampleBytes PortableBundle::readAsset (const juce::String& path) const
{
    if (! valid || ! sfizioso_manifest::isPackagePath (path)) return {};
    const auto it = assets.find (path.toStdString());
    return it == assets.end() ? SampleBytes{} : it->second;
}

} // namespace samplemachine
