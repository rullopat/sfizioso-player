#include <PortableBundle.h>

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using samplemachine::PortableBundle;

namespace
{

constexpr std::size_t kHeaderSize = 48;
constexpr std::size_t kEntrySize = 256;
constexpr std::uint32_t kBundleMagic = 0x42504D53;
constexpr std::uint16_t kBundleVersion = 2;
constexpr std::uint16_t kEncryptedFlag = 1u << 0;

enum class EntryType : std::uint16_t
{
    SfzText = 1,
    SampleWav = 2,
    SampleFlac = 3,
    MetadataJson = 4
};

struct Entry
{
    EntryType type;
    std::string name;
    std::vector<std::uint8_t> payload;
};

void writeU16 (std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t> (value);
    bytes[offset + 1] = static_cast<std::uint8_t> (value >> 8);
}

void writeU32 (std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t> (value >> (i * 8));
}

void writeU64 (std::vector<std::uint8_t>& bytes, std::size_t offset,
               std::uint64_t value)
{
    for (unsigned int i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::uint8_t> (value >> (i * 8));
}

std::vector<std::uint8_t> asBytes (const std::string& text)
{
    return { text.begin(), text.end() };
}

std::vector<std::uint8_t> makeBundle (const std::vector<Entry>& entries,
                                      std::uint16_t flags = 0)
{
    const auto tableEnd = kHeaderSize + entries.size() * kEntrySize;
    std::size_t totalSize = tableEnd;
    for (const auto& entry : entries)
        totalSize += entry.payload.size();

    std::vector<std::uint8_t> bytes (totalSize, 0);
    writeU32 (bytes, 0, kBundleMagic);
    writeU16 (bytes, 4, kBundleVersion);
    writeU16 (bytes, 6, flags);
    writeU32 (bytes, 8, static_cast<std::uint32_t> (entries.size()));
    writeU32 (bytes, 12, static_cast<std::uint32_t> (kHeaderSize));
    writeU32 (bytes, 16, static_cast<std::uint32_t> (kEntrySize));

    std::size_t payloadOffset = tableEnd;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto entryOffset = kHeaderSize + i * kEntrySize;
        const auto& entry = entries[i];
        writeU16 (bytes, entryOffset, static_cast<std::uint16_t> (entry.type));
        writeU64 (bytes, entryOffset + 8, payloadOffset);
        writeU64 (bytes, entryOffset + 16, entry.payload.size());

        const auto nameSize = std::min<std::size_t> (entry.name.size(), 231);
        std::copy_n (entry.name.begin(), nameSize,
                     bytes.begin() + entryOffset + 24);
        std::copy (entry.payload.begin(), entry.payload.end(),
                   bytes.begin() + payloadOffset);
        payloadOffset += entry.payload.size();
    }

    return bytes;
}

class TempDirectory
{
public:
    TempDirectory()
        : directory (juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile (
                             "sfizioso-player-bundle-tests", {}, false))
    {
        REQUIRE (directory.createDirectory().wasOk());
    }

    ~TempDirectory()
    {
        directory.deleteRecursively();
    }

    juce::File write (const std::string& name,
                      const std::vector<std::uint8_t>& bytes) const
    {
        const auto file = directory.getChildFile (name);
        if (bytes.empty())
            REQUIRE (file.create().wasOk());
        else
            REQUIRE (file.replaceWithData (bytes.data(), bytes.size()));
        return file;
    }

private:
    juce::File directory;
};

void checkBytes (PortableBundle::SampleBytes actual,
                 const std::vector<std::uint8_t>& expected)
{
    REQUIRE (actual.data != nullptr);
    REQUIRE (actual.size == expected.size());
    CHECK (std::memcmp (actual.data, expected.data(), expected.size()) == 0);
}

} // namespace

TEST_CASE ("PortableBundle loads valid SFZ, metadata, WAV, and FLAC entries",
           "[portable_bundle]")
{
    TempDirectory temp;
    const std::vector<std::uint8_t> wav{ 0x52, 0x49, 0x46, 0x46 };
    const std::vector<std::uint8_t> flac{ 0x66, 0x4c, 0x61, 0x43 };
    const auto bundleBytes = makeBundle (
        { { EntryType::MetadataJson, "presets.json",
            asBytes (
                R"({"instrumentName":"Test Instrument","patchName":"Warm Pad"})") },
          { EntryType::SampleWav, "samples\\tone.wav", wav },
          { EntryType::SfzText, "preset.sfz",
            asBytes ("<region> sample=samples/tone.wav\n") },
          { EntryType::SampleFlac, "samples/tone.flac", flac } });
    const auto file = temp.write ("valid.sfzbundle", bundleBytes);

    PortableBundle bundle;
    REQUIRE (bundle.loadFromFile (file));
    CHECK (bundle.isValid());
    CHECK (bundle.getError().isEmpty());
    CHECK (bundle.getSfzText() == "<region> sample=samples/tone.wav\n");
    CHECK (bundle.getInstrumentName() == "Test Instrument");
    CHECK (bundle.getPatchName() == "Warm Pad");
    CHECK (bundle.getVirtualPath() == "/__sfizioso_bundle__/valid.sfz");

    checkBytes (bundle.readSampleBytes ("samples/tone.wav"), wav);
    checkBytes (bundle.readSampleBytes ("another/path/tone.wav"), wav);
    checkBytes (bundle.readSampleBytes ("samples\\tone.flac"), flac);
    CHECK (bundle.readSampleBytes ("missing.wav").data == nullptr);
}

TEST_CASE ("PortableBundle rejects malformed and truncated containers",
           "[portable_bundle][malformed]")
{
    TempDirectory temp;
    PortableBundle bundle;

    SECTION ("empty file")
    {
        CHECK_FALSE (bundle.loadFromFile (temp.write ("empty.sfzbundle", {})));
        CHECK (bundle.getError().containsIgnoreCase ("header"));
    }

    SECTION ("truncated header")
    {
        auto bytes = makeBundle ({});
        bytes.resize (kHeaderSize - 1);
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("header.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("header"));
    }

    SECTION ("truncated entry table")
    {
        auto bytes = makeBundle ({ { EntryType::SfzText, "preset.sfz",
                                     asBytes ("<region> sample=*sine") } });
        bytes.resize (kHeaderSize + kEntrySize - 1);
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("table.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("header"));
    }

    SECTION ("payload extends beyond the file")
    {
        auto bytes = makeBundle ({ { EntryType::SfzText, "preset.sfz",
                                     asBytes ("<region> sample=*sine") } });
        writeU64 (bytes, kHeaderSize + 16, bytes.size());
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("overrun.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("entry"));
    }

    SECTION ("payload range arithmetic would overflow")
    {
        auto bytes = makeBundle ({ { EntryType::SfzText, "preset.sfz",
                                     asBytes ("<region> sample=*sine") } });
        writeU64 (bytes, kHeaderSize + 8,
                  std::numeric_limits<std::uint64_t>::max() - 3);
        writeU64 (bytes, kHeaderSize + 16, 8);
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("overflow.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("entry"));
    }

    SECTION ("payload points into the entry table")
    {
        auto bytes = makeBundle ({ { EntryType::SfzText, "preset.sfz",
                                     asBytes ("<region> sample=*sine") } });
        writeU64 (bytes, kHeaderSize + 8, kHeaderSize);
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("overlap.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("entry"));
    }

    SECTION ("entry name is not NUL-terminated")
    {
        auto bytes = makeBundle ({ { EntryType::SfzText, "preset.sfz",
                                     asBytes ("<region> sample=*sine") } });
        std::fill_n (bytes.begin() + kHeaderSize + 24, 232,
                     static_cast<std::uint8_t> ('x'));
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("entry-name.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("entry"));
    }

    SECTION ("SFZ payload is not UTF-8")
    {
        const auto bytes = makeBundle (
            { { EntryType::SfzText, "preset.sfz", { 0xc3, 0x28 } } });
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("sfz-utf8.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("UTF-8"));
    }
}

TEST_CASE ("PortableBundle rejects unsupported encryption and missing SFZ data",
           "[portable_bundle][malformed]")
{
    TempDirectory temp;

    SECTION ("encrypted bundle")
    {
        const auto bytes =
            makeBundle ({ { EntryType::SfzText, "preset.sfz",
                            asBytes ("<region> sample=*sine") } },
                        kEncryptedFlag);
        PortableBundle bundle;
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("encrypted.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("encrypted"));
    }

    SECTION ("bundle without an SFZ entry")
    {
        const auto bytes =
            makeBundle ({ { EntryType::SampleWav, "tone.wav", { 1, 2, 3 } } });
        PortableBundle bundle;
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("missing-sfz.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("no SFZ"));
    }
}

TEST_CASE ("PortableBundle rejects invalid metadata cleanly",
           "[portable_bundle][metadata][malformed]")
{
    TempDirectory temp;

    SECTION ("malformed JSON")
    {
        const auto bytes =
            makeBundle ({ { EntryType::SfzText, "preset.sfz",
                            asBytes ("<region> sample=*sine") },
                          { EntryType::MetadataJson, "presets.json",
                            asBytes ("{not-json") } });
        PortableBundle bundle;
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("bad-json.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("JSON object"));
    }

    SECTION ("metadata JSON is not an object")
    {
        const auto bytes = makeBundle (
            { { EntryType::SfzText, "preset.sfz",
                asBytes ("<region> sample=*sine") },
              { EntryType::MetadataJson, "presets.json", asBytes ("[]") } });
        PortableBundle bundle;
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("json-array.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("JSON object"));
    }

    SECTION ("metadata is not UTF-8")
    {
        const auto bytes = makeBundle (
            { { EntryType::SfzText, "preset.sfz",
                asBytes ("<region> sample=*sine") },
              { EntryType::MetadataJson, "presets.json", { 0xc3, 0x28 } } });
        PortableBundle bundle;
        CHECK_FALSE (
            bundle.loadFromFile (temp.write ("bad-utf8.sfzbundle", bytes)));
        CHECK (bundle.getError().containsIgnoreCase ("UTF-8"));
    }
}

TEST_CASE ("PortableBundle exposes no partial data after a malformed entry",
           "[portable_bundle][malformed]")
{
    TempDirectory temp;
    auto bytes =
        makeBundle ({ { EntryType::SampleWav, "tone.wav", { 1, 2, 3 } },
                      { EntryType::SfzText, "preset.sfz",
                        asBytes ("<region> sample=tone.wav") } });
    const auto secondEntry = kHeaderSize + kEntrySize;
    writeU64 (bytes, secondEntry + 8,
              std::numeric_limits<std::uint64_t>::max());
    writeU64 (bytes, secondEntry + 16, 2);

    PortableBundle bundle;
    CHECK_FALSE (bundle.loadFromFile (temp.write ("partial.sfzbundle", bytes)));
    CHECK_FALSE (bundle.isValid());
    CHECK (bundle.getSfzText().isEmpty());
    CHECK (bundle.readSampleBytes ("tone.wav").data == nullptr);
}
