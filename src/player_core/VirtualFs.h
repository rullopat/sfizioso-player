#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace samplemachine
{

// Interface for serving sample data from an in-memory source rather than disk.
//
// SMPL-31 ships this as a stub: PlayerEngine doesn't yet route sfizz sample
// reads through it. SMPL-33 wires it up to a real sfizz virtual filesystem
// callback so the encrypted/decrypted sample bytes from the rompler bundle
// can be served out of a heap buffer.
//
// Defined here so the rompler target can build against the interface from
// day one of SMPL-32, even though its first concrete use lands in SMPL-33.
class VirtualFs
{
public:
    virtual ~VirtualFs() = default;

    // Returns (bytes, length) for the requested path, or (nullptr, 0) if not found.
    // The returned pointer remains valid for the lifetime of the VirtualFs instance.
    virtual std::pair<const std::uint8_t*, std::size_t> readSample (const std::string& path) const = 0;
};

} // namespace samplemachine
