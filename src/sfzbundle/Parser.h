#pragma once

#include <sfzbundle/Format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace sfzbundle
{

struct ByteView
{
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

struct HeaderView
{
    std::uint16_t flags = 0;
    std::uint32_t entryCount = 0;
    std::size_t tableEnd = 0;
    std::array<std::uint8_t, 16> iv {};
};

struct EntryView
{
    BundleEntryType type = BundleEntryType::Unknown;
    std::string_view name;
    ByteView payload;
};

enum class ParseErrorCode
{
    None,
    NullData,
    TruncatedHeader,
    InvalidMagic,
    UnsupportedVersion,
    HeaderSizeMismatch,
    EntrySizeMismatch,
    TruncatedEntryTable,
    PayloadOverlapsEntryTable,
    PayloadOutOfRange,
    UnterminatedEntryName,
    InvalidEntryNameUtf8
};

struct ParseError
{
    static constexpr std::size_t noEntry = static_cast<std::size_t> (-1);

    ParseErrorCode code = ParseErrorCode::None;
    std::size_t entryIndex = noEntry;

    explicit operator bool() const noexcept { return code != ParseErrorCode::None; }
};

const char* parseErrorMessage (ParseErrorCode code) noexcept;

// Strict UTF-8 validation for entry names and host-interpreted text payloads.
bool isValidUtf8 (ByteView bytes) noexcept;

// Inspects only the fixed plaintext header and validates that the declared
// entry table can fit in the supplied buffer. Encrypted consumers use this to
// obtain flags and IV before decrypting bytes after kBundleHeaderSize.
bool inspectHeader (const void* data, std::size_t size,
                    HeaderView& header, ParseError& error) noexcept;

// A validated non-owning view. The source byte buffer must outlive this object.
// parse() is transactional: failure clears all header and entry state.
class BundleView
{
public:
    bool parse (const void* data, std::size_t size);

    bool isValid() const noexcept { return valid; }
    const HeaderView& header() const noexcept { return parsedHeader; }
    const std::vector<EntryView>& entries() const noexcept { return parsedEntries; }
    const ParseError& error() const noexcept { return parseError; }

private:
    bool valid = false;
    HeaderView parsedHeader;
    std::vector<EntryView> parsedEntries;
    ParseError parseError;
};

} // namespace sfzbundle
