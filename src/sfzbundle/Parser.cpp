#include <sfzbundle/Parser.h>

#include <algorithm>
#include <utility>

namespace sfzbundle
{
namespace
{

std::uint16_t readU16 (const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint16_t> (bytes[0])
           | (static_cast<std::uint16_t> (bytes[1]) << 8);
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

bool isContinuation (std::uint8_t byte) noexcept
{
    return (byte & 0xc0) == 0x80;
}

bool validateUtf8 (const std::uint8_t* bytes, std::size_t size) noexcept
{
    std::size_t index = 0;
    while (index < size)
    {
        const auto first = bytes[index++];
        if (first <= 0x7f)
            continue;

        if (first >= 0xc2 && first <= 0xdf)
        {
            if (index >= size || ! isContinuation (bytes[index]))
                return false;
            ++index;
            continue;
        }

        if (first >= 0xe0 && first <= 0xef)
        {
            if (size - index < 2)
                return false;
            const auto second = bytes[index];
            const auto third = bytes[index + 1];
            if (! isContinuation (third)
                || (first == 0xe0 && (second < 0xa0 || second > 0xbf))
                || (first == 0xed && (second < 0x80 || second > 0x9f))
                || ((first != 0xe0 && first != 0xed) && ! isContinuation (second)))
                return false;
            index += 2;
            continue;
        }

        if (first >= 0xf0 && first <= 0xf4)
        {
            if (size - index < 3)
                return false;
            const auto second = bytes[index];
            if ((first == 0xf0 && (second < 0x90 || second > 0xbf))
                || (first == 0xf4 && (second < 0x80 || second > 0x8f))
                || ((first != 0xf0 && first != 0xf4) && ! isContinuation (second))
                || ! isContinuation (bytes[index + 1])
                || ! isContinuation (bytes[index + 2]))
                return false;
            index += 3;
            continue;
        }

        return false;
    }

    return true;
}

ParseError makeEntryError (ParseErrorCode code, std::size_t index) noexcept
{
    return { code, index };
}

} // namespace

bool isValidUtf8 (ByteView bytes) noexcept
{
    return bytes.size == 0
           || (bytes.data != nullptr && validateUtf8 (bytes.data, bytes.size));
}

const char* parseErrorMessage (ParseErrorCode code) noexcept
{
    switch (code)
    {
        case ParseErrorCode::None: return "no error";
        case ParseErrorCode::NullData: return "bundle data is null";
        case ParseErrorCode::TruncatedHeader: return "bundle header is truncated";
        case ParseErrorCode::InvalidMagic: return "bundle magic is invalid";
        case ParseErrorCode::UnsupportedVersion: return "bundle version is unsupported";
        case ParseErrorCode::HeaderSizeMismatch: return "bundle header size is invalid";
        case ParseErrorCode::EntrySizeMismatch: return "bundle entry size is invalid";
        case ParseErrorCode::TruncatedEntryTable: return "bundle entry table is truncated";
        case ParseErrorCode::PayloadOverlapsEntryTable: return "bundle payload overlaps the entry table";
        case ParseErrorCode::PayloadOutOfRange: return "bundle payload is outside the input bytes";
        case ParseErrorCode::UnterminatedEntryName: return "bundle entry name is not NUL-terminated";
        case ParseErrorCode::InvalidEntryNameUtf8: return "bundle entry name is not valid UTF-8";
    }

    return "unknown bundle parse error";
}

bool inspectHeader (const void* data, std::size_t size,
                    HeaderView& header, ParseError& error) noexcept
{
    header = {};
    error = {};

    if (data == nullptr)
    {
        error.code = ParseErrorCode::NullData;
        return false;
    }
    if (size < kBundleHeaderSize)
    {
        error.code = ParseErrorCode::TruncatedHeader;
        return false;
    }

    const auto* bytes = static_cast<const std::uint8_t*> (data);
    if (readU32 (bytes) != kBundleMagic)
    {
        error.code = ParseErrorCode::InvalidMagic;
        return false;
    }
    if (readU16 (bytes + 4) != kBundleVersion)
    {
        error.code = ParseErrorCode::UnsupportedVersion;
        return false;
    }
    if (readU32 (bytes + 12) != kBundleHeaderSize)
    {
        error.code = ParseErrorCode::HeaderSizeMismatch;
        return false;
    }
    if (readU32 (bytes + 16) != kBundleEntrySize)
    {
        error.code = ParseErrorCode::EntrySizeMismatch;
        return false;
    }

    const auto entryCount = readU32 (bytes + 8);
    // Subtraction-first validation avoids overflowing an attacker-controlled
    // entryCount while computing the end of the table.
    if (entryCount > (size - kBundleHeaderSize) / kBundleEntrySize)
    {
        error.code = ParseErrorCode::TruncatedEntryTable;
        return false;
    }

    header.flags = readU16 (bytes + 6);
    header.entryCount = entryCount;
    header.tableEnd = kBundleHeaderSize
                      + static_cast<std::size_t> (entryCount) * kBundleEntrySize;
    std::copy_n (bytes + 20, header.iv.size(), header.iv.begin());
    return true;
}

bool BundleView::parse (const void* data, std::size_t size)
{
    valid = false;
    parsedHeader = {};
    parsedEntries.clear();
    parseError = {};

    HeaderView candidateHeader;
    if (! inspectHeader (data, size, candidateHeader, parseError))
        return false;

    const auto* bytes = static_cast<const std::uint8_t*> (data);
    const auto size64 = static_cast<std::uint64_t> (size);
    std::vector<EntryView> candidateEntries;
    candidateEntries.reserve (candidateHeader.entryCount);

    for (std::size_t index = 0; index < candidateHeader.entryCount; ++index)
    {
        const auto* entry = bytes + kBundleHeaderSize + index * kBundleEntrySize;
        const auto offset = readU64 (entry + 8);
        const auto length = readU64 (entry + 16);

        if (offset < candidateHeader.tableEnd)
        {
            parseError = makeEntryError (
                ParseErrorCode::PayloadOverlapsEntryTable, index);
            return false;
        }
        // Validate with subtraction rather than offset + length, which can
        // wrap for hostile 64-bit values.
        if (offset > size64 || length > size64 - offset)
        {
            parseError = makeEntryError (ParseErrorCode::PayloadOutOfRange, index);
            return false;
        }

        const auto* name = entry + 24;
        std::size_t nameLength = 0;
        while (nameLength < kBundleEntryNameSize && name[nameLength] != 0)
            ++nameLength;
        if (nameLength == kBundleEntryNameSize)
        {
            parseError = makeEntryError (ParseErrorCode::UnterminatedEntryName, index);
            return false;
        }
        if (! isValidUtf8 ({ name, nameLength }))
        {
            parseError = makeEntryError (ParseErrorCode::InvalidEntryNameUtf8, index);
            return false;
        }

        candidateEntries.push_back ({
            static_cast<BundleEntryType> (readU16 (entry)),
            { reinterpret_cast<const char*> (name), nameLength },
            { bytes + static_cast<std::size_t> (offset),
              static_cast<std::size_t> (length) }
        });
    }

    parsedHeader = candidateHeader;
    parsedEntries = std::move (candidateEntries);
    valid = true;
    return true;
}

} // namespace sfzbundle
