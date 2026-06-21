#include "PortableBundle.h"

#include <sfizioso.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace samplemachine
{
namespace
{

inline constexpr std::uint32_t kBundleMagic   = 0x42504D53;
inline constexpr std::uint16_t kBundleVersion = 2;
inline constexpr std::uint16_t kBundleFlagEncrypted = 1u << 0;

enum class BundleEntryType : std::uint16_t
{
    Unknown      = 0,
    SfzText      = 1,
    SampleWav    = 2,
    SampleFlac   = 3,
    MetadataJson = 4
};

#pragma pack(push, 1)
struct BundleHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t flags;
    std::uint32_t entryCount;
    std::uint32_t headerSize;
    std::uint32_t entrySize;
    std::uint8_t  iv[16];
    std::uint8_t  reserved[12];
};

struct BundleEntry
{
    std::uint16_t type;
    std::uint16_t reserved0;
    std::uint32_t reserved1;
    std::uint64_t offset;
    std::uint64_t length;
    std::uint8_t  name[232];
};
#pragma pack(pop)

static_assert (sizeof (BundleHeader) == 48);
static_assert (sizeof (BundleEntry) == 256);

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

juce::String entryName (const BundleEntry& e)
{
    const auto* p = reinterpret_cast<const char*> (e.name);
    std::size_t n = 0;
    while (n < sizeof (e.name) && p[n] != '\0')
        ++n;
    return juce::String::fromUTF8 (p, static_cast<int> (n));
}

const BundleHeader* readHeader (const std::uint8_t* bytes, std::size_t size)
{
    if (size < sizeof (BundleHeader))
        return nullptr;

    auto* h = reinterpret_cast<const BundleHeader*> (bytes);
    if (h->magic != kBundleMagic
        || h->version != kBundleVersion
        || h->headerSize != sizeof (BundleHeader)
        || h->entrySize != sizeof (BundleEntry))
        return nullptr;

    const auto tableEnd = sizeof (BundleHeader) + std::uint64_t (h->entryCount) * sizeof (BundleEntry);
    if (tableEnd > size)
        return nullptr;

    return h;
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
    bundleBytes.clear();
    sfzText.clear();
    instrumentName.clear();
    patchName.clear();
    samples.clear();

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
    const auto* header = readHeader (bytes, size);
    if (header == nullptr)
    {
        errorMessage = "Invalid .sfzbundle header";
        return false;
    }

    if ((header->flags & kBundleFlagEncrypted) != 0)
    {
        errorMessage = "Encrypted .sfzbundle files are not supported by this player";
        return false;
    }

    const auto* table = reinterpret_cast<const BundleEntry*> (bytes + sizeof (BundleHeader));
    juce::String metadataJson;

    for (std::uint32_t i = 0; i < header->entryCount; ++i)
    {
        const auto& e = table[i];
        if (e.offset + e.length > size)
        {
            errorMessage = "Bundle entry overruns file";
            return false;
        }

        const auto type = static_cast<BundleEntryType> (e.type);
        const auto name = entryName (e);

        if (type == BundleEntryType::SampleWav || type == BundleEntryType::SampleFlac)
        {
            const auto key = normaliseKey (name.toStdString());
            const SampleBytes sample { bytes + e.offset, static_cast<std::size_t> (e.length) };
            samples[key] = sample;
            samples[basenameKey (key)] = sample;
        }
        else if (type == BundleEntryType::MetadataJson)
        {
            metadataJson = juce::String::fromUTF8 (reinterpret_cast<const char*> (bytes + e.offset),
                                                   static_cast<int> (e.length));
        }
        else if (type == BundleEntryType::SfzText && sfzText.isEmpty())
        {
            sfzText = juce::String::fromUTF8 (reinterpret_cast<const char*> (bytes + e.offset),
                                              static_cast<int> (e.length));
        }
    }

    if (sfzText.isEmpty())
    {
        errorMessage = "Bundle contains no SFZ preset";
        return false;
    }

    if (metadataJson.isNotEmpty())
    {
        auto parsed = juce::JSON::parse (metadataJson);
        if (auto* obj = parsed.getDynamicObject())
        {
            instrumentName = obj->getProperty ("instrumentName").toString();
            patchName = obj->getProperty ("patchName").toString();
        }
    }

    return true;
}

} // namespace samplemachine
