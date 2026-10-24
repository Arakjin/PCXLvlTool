# OverThrust OVL container v1

`.OVL` is OverThrust's native, deterministic, single-file level container. It
is not ZIP-compatible and has no directory or filename layer. All integers are
unsigned little-endian values.

## Header

```text
Offset  Size  Field
0       8     magic: "OVLPKG1\0"
8       4     container version: 1
12      4     chunk count
16      8     chunk-table offset: 32
24      8     payload-data offset: 32 + chunk_count * 40
```

## Chunk table

Each 40-byte entry contains:

```text
Size  Field
8     ASCII chunk ID, padded with zero bytes
8     absolute payload offset
8     stored payload size
8     uncompressed payload size (equal to stored size in version 1)
4     CRC-32 of the payload
4     flags, zero in version 1
```

Version 1 stores chunks contiguously in table order without container-level
compression. Embedded formats may perform their own compression. Every offset,
length, checksum, flag, and the final file size must be validated before a
payload is used.

The standard chunk order and payloads are:

| ID | Required | Payload |
|---|---:|---|
| `MANIFEST` | yes | UTF-8 JSON manifest |
| `MATERIAL` | yes | ordinary indexed PCX material raster |
| `BACKGRND` | yes | OverThrust Layered PCX (`.OPCX`) background |
| `PRESENT` | no | canonical RGBA PNG presentation background |
| `OBJECTS` | yes | UTF-8 JSON object data |

The manifest refers to chunk IDs rather than filenames. There is deliberately
no compatibility path for the earlier folder or ZIP prototypes.

PCX Level Tool opens this native format directly. A successful read validates
the header, contiguous chunk table, supported chunk set, bounds, flags and
CRC-32 values before decoding the manifest, objects, PCX, OPCX and optional
PNG payloads.
