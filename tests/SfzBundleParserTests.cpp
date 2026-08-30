#include <sfzbundle/Parser.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

using sfzbundle::BundleEntryType;

struct Entry
{
    BundleEntryType type;
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

std::vector<std::uint8_t> makeBundle (const std::vector<Entry>& entries,
                                      std::uint16_t flags = 0)
{
    const auto tableEnd = sfzbundle::kBundleHeaderSize
                          + entries.size() * sfzbundle::kBundleEntrySize;
    std::size_t totalSize = tableEnd;
    for (const auto& entry : entries)
        totalSize += entry.payload.size();

    std::vector<std::uint8_t> bytes (totalSize, 0);
    writeU32 (bytes, 0, sfzbundle::kBundleMagic);
    writeU16 (bytes, 4, sfzbundle::kBundleVersion);
    writeU16 (bytes, 6, flags);
    writeU32 (bytes, 8, static_cast<std::uint32_t> (entries.size()));
    writeU32 (bytes, 12,
              static_cast<std::uint32_t> (sfzbundle::kBundleHeaderSize));
    writeU32 (bytes, 16,
              static_cast<std::uint32_t> (sfzbundle::kBundleEntrySize));

    std::size_t payloadOffset = tableEnd;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const auto entryOffset = sfzbundle::kBundleHeaderSize
                                 + index * sfzbundle::kBundleEntrySize;
        const auto& entry = entries[index];
        writeU16 (bytes, entryOffset, static_cast<std::uint16_t> (entry.type));
        writeU64 (bytes, entryOffset + 8, payloadOffset);
        writeU64 (bytes, entryOffset + 16, entry.payload.size());
        std::copy (entry.name.begin(), entry.name.end(),
                   bytes.begin() + entryOffset + 24);
        std::copy (entry.payload.begin(), entry.payload.end(),
                   bytes.begin() + payloadOffset);
        payloadOffset += entry.payload.size();
    }

    return bytes;
}

} // namespace

TEST_CASE ("sfzbundle parser exposes validated non-owning entry views",
           "[sfzbundle]")
{
    const std::string utf8Name = "samples/caf\xc3\xa9.wav";
    const std::vector<std::uint8_t> payload { 1, 2, 3, 4 };
    auto bytes = makeBundle ({ { BundleEntryType::SampleWav,
                                 utf8Name, payload } },
                             sfzbundle::kBundleFlagFlacInside);
    for (std::size_t index = 0; index < 16; ++index)
        bytes[20 + index] = static_cast<std::uint8_t> (index + 1);

    sfzbundle::HeaderView inspected;
    sfzbundle::ParseError inspectError;
    REQUIRE (sfzbundle::inspectHeader (
        bytes.data(), bytes.size(), inspected, inspectError));
    CHECK_FALSE (inspectError);
    CHECK (inspected.flags == sfzbundle::kBundleFlagFlacInside);
    CHECK (inspected.entryCount == 1);
    CHECK (inspected.tableEnd == sfzbundle::kBundleHeaderSize
                                  + sfzbundle::kBundleEntrySize);
    CHECK (inspected.iv.front() == 1);
    CHECK (inspected.iv.back() == 16);

    sfzbundle::BundleView bundle;
    REQUIRE (bundle.parse (bytes.data(), bytes.size()));
    REQUIRE (bundle.isValid());
    REQUIRE (bundle.entries().size() == 1);
    const auto& entry = bundle.entries().front();
    CHECK (entry.type == BundleEntryType::SampleWav);
    CHECK (entry.name == utf8Name);
    REQUIRE (entry.payload.size == payload.size());
    CHECK (std::equal (payload.begin(), payload.end(), entry.payload.data));
}

TEST_CASE ("sfzbundle parser rejects malformed tables and payload ranges",
           "[sfzbundle][malformed]")
{
    const Entry sfz { BundleEntryType::SfzText, "preset.sfz", { '<', 'r', '>' } };

    SECTION ("truncated header")
    {
        auto bytes = makeBundle ({});
        bytes.resize (sfzbundle::kBundleHeaderSize - 1);
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code == sfzbundle::ParseErrorCode::TruncatedHeader);
    }

    SECTION ("truncated table")
    {
        auto bytes = makeBundle ({ sfz });
        bytes.resize (sfzbundle::kBundleHeaderSize
                      + sfzbundle::kBundleEntrySize - 1);
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code
               == sfzbundle::ParseErrorCode::TruncatedEntryTable);
    }

    SECTION ("payload overlaps table")
    {
        auto bytes = makeBundle ({ sfz });
        writeU64 (bytes, sfzbundle::kBundleHeaderSize + 8,
                  sfzbundle::kBundleHeaderSize);
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code
               == sfzbundle::ParseErrorCode::PayloadOverlapsEntryTable);
        CHECK (bundle.error().entryIndex == 0);
    }

    SECTION ("payload addition would overflow")
    {
        auto bytes = makeBundle ({ sfz });
        writeU64 (bytes, sfzbundle::kBundleHeaderSize + 8,
                  std::numeric_limits<std::uint64_t>::max() - 2);
        writeU64 (bytes, sfzbundle::kBundleHeaderSize + 16, 8);
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code
               == sfzbundle::ParseErrorCode::PayloadOutOfRange);
        CHECK (bundle.error().entryIndex == 0);
    }
}

TEST_CASE ("sfzbundle parser bounds and validates entry names",
           "[sfzbundle][malformed]")
{
    const Entry sfz { BundleEntryType::SfzText, "preset.sfz", { '<', 'r', '>' } };

    SECTION ("unterminated name")
    {
        auto bytes = makeBundle ({ sfz });
        std::fill_n (bytes.begin() + sfzbundle::kBundleHeaderSize + 24,
                     sfzbundle::kBundleEntryNameSize,
                     static_cast<std::uint8_t> ('x'));
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code
               == sfzbundle::ParseErrorCode::UnterminatedEntryName);
    }

    SECTION ("invalid UTF-8 name")
    {
        auto bytes = makeBundle ({ sfz });
        bytes[sfzbundle::kBundleHeaderSize + 24] = 0xc3;
        bytes[sfzbundle::kBundleHeaderSize + 25] = 0x28;
        bytes[sfzbundle::kBundleHeaderSize + 26] = 0;
        sfzbundle::BundleView bundle;
        CHECK_FALSE (bundle.parse (bytes.data(), bytes.size()));
        CHECK (bundle.error().code
               == sfzbundle::ParseErrorCode::InvalidEntryNameUtf8);
    }
}

TEST_CASE ("sfzbundle parser clears a previous view after failure",
           "[sfzbundle][malformed]")
{
    auto validBytes = makeBundle (
        { { BundleEntryType::SfzText, "preset.sfz", { '<', 'r', '>' } } });
    sfzbundle::BundleView bundle;
    REQUIRE (bundle.parse (validBytes.data(), validBytes.size()));
    REQUIRE_FALSE (bundle.entries().empty());

    auto invalidBytes = validBytes;
    writeU64 (invalidBytes, sfzbundle::kBundleHeaderSize + 8,
              std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE (bundle.parse (invalidBytes.data(), invalidBytes.size()));
    CHECK_FALSE (bundle.isValid());
    CHECK (bundle.entries().empty());
    CHECK (bundle.header().entryCount == 0);
    CHECK (bundle.error().entryIndex == 0);
    CHECK (std::string (sfzbundle::parseErrorMessage (bundle.error().code))
           == "bundle payload is outside the input bytes");
}
