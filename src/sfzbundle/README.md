# sfzbundle

`sfzbundle` is the dependency-free C++17 definition and structural parser for
Sfizioso Player bundle format version 2. It is licensed under BSD-2-Clause.

Consumers link the CMake target `sfizioso_player::sfzbundle` and include
`<sfzbundle/Parser.h>` or `<sfzbundle/Format.h>`.

## Version-2 layout

All integers are little-endian.

```text
[48-byte BundleHeader]
[256-byte BundleEntry] * entryCount
[payload bytes referenced by each entry]
```

The header contains `SMPB` magic, version, flags, record sizes, entry count, and
a 16-byte IV. Every entry contains a type, absolute payload offset and length,
and a 232-byte NUL-terminated UTF-8 name.

When the encrypted flag is set, the fixed header remains plaintext and bytes
starting at offset 48 are AES-CTR encrypted. Cryptography and key policy are not
part of this library. An encrypted consumer should:

1. Call `inspectHeader()` on the original bytes.
2. Read the validated flags and IV.
3. Decrypt bytes beginning at `kBundleHeaderSize` into caller-owned storage.
4. Call `BundleView::parse()` on the decrypted buffer.

`BundleView` borrows the source buffer. The caller must keep that storage alive
and immovable while using entry names or payload views.

## Validation boundary

`BundleView::parse()` validates:

- magic, version, and fixed record sizes;
- subtraction-first entry-table sizing;
- payload ranges without `offset + length` overflow;
- payload exclusion from the header/entry table;
- bounded NUL termination and strict UTF-8 for entry names;
- transactional publication of header and entry views.

Payload meaning remains a host concern. The library does not parse SFZ, JSON,
audio, images, fonts, or encryption keys. Hosts can use `isValidUtf8()` before
interpreting textual payloads.
