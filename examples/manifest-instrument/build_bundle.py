#!/usr/bin/env python3
"""Package this trusted demonstration directory as a plaintext SMPB v2 bundle.
Usage: python build_bundle.py /tmp/evening-signals.sfzbundle
This is an example writer, not an untrusted-package import tool.
"""
import pathlib
import struct
import sys

root = pathlib.Path(__file__).resolve().parent
entries = [(1, 'instrument.sfz'), (4, 'instrument.json'), (8, 'artwork/background.png')]
payloads = [root.joinpath(name).read_bytes() for _, name in entries]
header = struct.pack('<IHHIII16s12s', 0x42504D53, 2, 0, len(entries), 48, 256, bytes(16), bytes(12))
offset = 48 + 256 * len(entries)
table = bytearray()
for (kind, name), data in zip(entries, payloads):
    table.extend(struct.pack('<HHIQQ232s', kind, 0, 0, offset, len(data), name.encode()))
    offset += len(data)
pathlib.Path(sys.argv[1]).write_bytes(header + table + b''.join(payloads))
