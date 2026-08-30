#pragma once

// Canonical .sfzbundle version-2 byte layout.
//
// This header intentionally has no JUCE or sfizioso dependency. The packed
// structures are provided for writers which construct the serialized records;
// readers must use Parser.h rather than casting untrusted bytes to these types.
// All multi-byte fields are little-endian and entry names are NUL-terminated
// UTF-8.

#include <cstddef>
#include <cstdint>

namespace sfzbundle
{

inline constexpr std::uint32_t kBundleMagic = 0x42504D53; // 'SMPB', little-endian
inline constexpr std::uint16_t kBundleVersion = 2;

inline constexpr std::uint16_t kBundleFlagEncrypted = 1u << 0;
inline constexpr std::uint16_t kBundleFlagFlacInside = 1u << 1;

enum class BundleEntryType : std::uint16_t
{
    Unknown      = 0,
    SfzText      = 1,
    SampleWav    = 2,
    SampleFlac   = 3,
    MetadataJson = 4,
    LogoPng      = 5,
    WordmarkSvg  = 6,
    FontWoff2    = 7
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
    std::uint8_t iv[16];
    std::uint8_t reserved[12];
};

struct BundleEntry
{
    std::uint16_t type;
    std::uint16_t reserved0;
    std::uint32_t reserved1;
    std::uint64_t offset;
    std::uint64_t length;
    std::uint8_t name[232];
};

#pragma pack(pop)

inline constexpr std::size_t kBundleHeaderSize = 48;
inline constexpr std::size_t kBundleEntrySize = 256;
inline constexpr std::size_t kBundleEntryNameSize = 232;

static_assert (sizeof (BundleHeader) == kBundleHeaderSize,
               "BundleHeader layout drift");
static_assert (sizeof (BundleEntry) == kBundleEntrySize,
               "BundleEntry layout drift");

} // namespace sfzbundle
