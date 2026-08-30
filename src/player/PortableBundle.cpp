#include "PortableBundle.h"

#include <sfizioso.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

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

std::uint16_t readU16 (const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint16_t> (bytes[0])
           | static_cast<std::uint16_t> (bytes[1] << 8);
}

std::uint32_t readU32 (const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t> (bytes[0])
           | (static_cast<std::uint32_t> (bytes[1]) << 8)
           | (static_cast<std::uint32_t> (bytes[2]) << 16)
           | (static_cast<std::uint32_t> (bytes[3]) << 24);
}

std::uint64_t readU64 (const std::uint8_t* bytes) noexcept
{
    std::uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t> (bytes[i]) << (i * 8);
    return value;
}

struct HeaderView
{
    std::uint16_t flags = 0;
    std::uint32_t entryCount = 0;
    std::size_t tableEnd = 0;
};

bool readHeader (const std::uint8_t* bytes, std::size_t size, HeaderView& result) noexcept
{
    if (bytes == nullptr || size < sizeof (BundleHeader))
        return false;

    if (readU32 (bytes) != kBundleMagic
        || readU16 (bytes + 4) != kBundleVersion
        || readU32 (bytes + 12) != sizeof (BundleHeader)
        || readU32 (bytes + 16) != sizeof (BundleEntry))
        return false;

    const auto entryCount = readU32 (bytes + 8);

    // Subtraction-first validation avoids overflowing while computing the end
    // of an attacker-controlled entry table.
    if (entryCount > (size - sizeof (BundleHeader)) / sizeof (BundleEntry))
        return false;

    result.flags = readU16 (bytes + 6);
    result.entryCount = entryCount;
    result.tableEnd = sizeof (BundleHeader)
                      + static_cast<std::size_t> (entryCount) * sizeof (BundleEntry);
    return true;
}

struct EntryView
{
    BundleEntryType type = BundleEntryType::Unknown;
    std::string name;
    const std::uint8_t* data = nullptr;
    std::size_t length = 0;
};

bool readEntry (const std::uint8_t* bytes, std::size_t size,
                const HeaderView& header, std::uint32_t index,
                EntryView& result)
{
    const auto* entry = bytes + sizeof (BundleHeader)
                        + static_cast<std::size_t> (index) * sizeof (BundleEntry);
    const auto offset = readU64 (entry + 8);
    const auto length = readU64 (entry + 16);
    const auto size64 = static_cast<std::uint64_t> (size);

    // Validate with subtraction rather than offset + length, which can wrap.
    // Payloads must also begin after the complete entry table.
    if (offset < header.tableEnd || offset > size64 || length > size64 - offset)
        return false;

    const auto* name = entry + 24;
    std::size_t nameLength = 0;
    while (nameLength < sizeof (BundleEntry::name) && name[nameLength] != 0)
        ++nameLength;
    if (nameLength == sizeof (BundleEntry::name))
        return false;

    result.type = static_cast<BundleEntryType> (readU16 (entry));
    result.name.assign (reinterpret_cast<const char*> (name), nameLength);
    result.data = bytes + static_cast<std::size_t> (offset);
    result.length = static_cast<std::size_t> (length);
    return true;
}

bool readUtf8 (const EntryView& entry, juce::String& result)
{
    if (entry.length > static_cast<std::size_t> (std::numeric_limits<int>::max()))
        return false;

    const auto length = static_cast<int> (entry.length);
    const auto* text = reinterpret_cast<const char*> (entry.data);
    if (! juce::CharPointer_UTF8::isValidString (text, length))
        return false;

    result = juce::String::fromUTF8 (text, length);
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
    HeaderView header;
    if (! readHeader (bytes, size, header))
    {
        errorMessage = "Invalid .sfzbundle header";
        return false;
    }

    if ((header.flags & kBundleFlagEncrypted) != 0)
    {
        errorMessage = "Encrypted .sfzbundle files are not supported by this player";
        return false;
    }

    // Build a complete temporary view first. A malformed later entry must not
    // expose pointers from an otherwise-invalid bundle through readSampleBytes.
    std::unordered_map<std::string, SampleBytes> parsedSamples;
    juce::String parsedSfzText;
    juce::String metadataJson;
    bool hasMetadata = false;

    for (std::uint32_t i = 0; i < header.entryCount; ++i)
    {
        EntryView entry;
        if (! readEntry (bytes, size, header, i, entry))
        {
            errorMessage = "Invalid bundle entry at index " + juce::String (i);
            return false;
        }

        if (entry.type == BundleEntryType::SampleWav
            || entry.type == BundleEntryType::SampleFlac)
        {
            const auto key = normaliseKey (entry.name);
            if (key.empty())
            {
                errorMessage = "Bundle sample entry has an empty name at index " + juce::String (i);
                return false;
            }

            const SampleBytes sample { entry.data, entry.length };
            parsedSamples[key] = sample;
            parsedSamples[basenameKey (key)] = sample;
        }
        else if (entry.type == BundleEntryType::MetadataJson)
        {
            hasMetadata = true;
            if (! readUtf8 (entry, metadataJson))
            {
                errorMessage = "Bundle metadata is not valid UTF-8";
                return false;
            }
        }
        else if (entry.type == BundleEntryType::SfzText && parsedSfzText.isEmpty())
        {
            if (! readUtf8 (entry, parsedSfzText))
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
