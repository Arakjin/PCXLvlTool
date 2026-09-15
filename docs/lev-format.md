# V-Wing `.LEV` format notes

This is an evidence log, not yet a format specification. Meanings are not
assigned to byte ranges until they can be demonstrated with multiple files or
controlled output from the original converter.

## Evidence status

- **Confirmed:** directly observed in multiple reference files.
- **Strongly suspected:** supported by multiple observations but still needs a
  controlled converter test.
- **Unknown:** observed bytes with no established meaning.

## Reference corpus

| File | Size (bytes) | ASCII text beginning at offset `0x0002` |
|------|-------------:|-----------------------------------------|
| `LEVEL1.LEV` | 82,982 | `TWILIGHT ZONE` |
| `LEVEL2.LEV` | 74,986 | `TWIN CITIES` |
| `LEVEL3.LEV` | 70,888 | `COLOSSUS OF WINORG` |
| `LEVEL4.LEV` | 66,278 | `THE ABYSS` |
| `LEVEL5.LEV` | 89,479 | `JUNGLE` |
| `LEVEL6.LEV` | 62,845 | `CITADEL OF YOQU` |
| `LEVEL7.LEV` | 150,591 | `OLD SPACESTATION` |
| `LEVEL8.LEV` | 58,977 | `CITY GREYEND` |
| `LEVEL9.LEV` | 66,794 | `DUH CAVES` |
| `LEVEL10.LEV` | 55,939 | `SKY CITY` |

The files are read from the parent V-Wing directory and remain unmodified.

## Byte layout

| Offset | Size | Status | Observation |
|-------:|-----:|--------|-------------|
| `0x0000` | 2 | Confirmed bytes, unknown meaning | Every corpus file and every converter output begins with `76 07`. |
| `0x0002` | 21 | Confirmed for converter 1.91 | Level-name area: up to 20 bytes followed by NUL. Bytes after the NUL through `0x0016` are spaces. Empty, 19-byte, and 20-byte names were tested. |
| `0x0017` | 2 | Confirmed for converter 1.91 | Zero bytes in controlled classic output; meaning unknown. |
| `0x0019` | 103 | Confirmed bytes, unknown meaning | Constant classic header tail through `0x007F`. The writer reproduces these bytes exactly. |
| `0x0000` | 128 | Confirmed | V-Wing-specific header written in place of the input PCX header. |
| `0x0080` | variable | Confirmed for converter 1.91 output | PCX RLE image stream. The converter copies every byte from input PCX offset 128 through EOF without changes. |
| EOF - 769 | 1 | Confirmed | Standard 256-color PCX palette marker `0C`. It is copied unchanged by the converter and is present in all ten reference levels. |
| EOF - 768 | 768 | Confirmed | 256 RGB palette entries, copied byte-for-byte from the PCX input by converter 1.91. |

The `LEVEL3.LEV` name area differs from the other nine samples around offsets
`0x0015` through `0x0018`. This is recorded as an anomaly only; it is not yet
evidence for a variable-length field.

Nine freeware levels begin the post-name constant byte sequence at offset
`0x0018`, while `LEVEL3.LEV` and all controlled converter 1.91 outputs begin
the corresponding sequence at `0x0019`. This is evidence of a header-layout
difference, but it is not yet enough to assign version identifiers or field
meanings.

## Controlled converter results

The original `CONV.EXE` was run under DOSBox against 15 deterministic fixtures:

- uniform indices 0, 1, and 57
- a single changed pixel at `(0,0)`, `(1,0)`, `(639,0)`, `(0,1)`, `(0,799)`,
  and `(639,799)`
- vertical stripes, horizontal stripes, checkerboard, long runs, and
  deterministic random pixels
- identical pixels with one palette entry changed

For every fixture, the `.LEV` output size equals the PCX input size and the
entire range from offset `0x0080` through EOF is byte-for-byte identical. This
confirms that converter 1.91 preserves the standard PCX RLE stream, palette
marker, and palette while replacing only the 128-byte header.

Two conversions of the same PCX with level names `NAME A` and `NAME B` differ
at exactly offset `0x0007`; all other bytes are identical. Further tests with
empty, 19-byte, and 20-byte names confirmed the classic name-area layout.

The classic writer was checked against 20 controlled converter outputs. Its
complete output, including header, RLE stream, palette marker, and palette, was
binary-identical in all 20 cases. Rewriting each of the ten freeware reference
levels with the classic header also preserved decoded pixels and palette
exactly, although the resulting header is intentionally not expected to match
the newer freeware header byte-for-byte.

## Converter documentation facts

`CONVERT.TXT`, shipped with converter version 1.91, states that converter input
is a 640 x 800, 256-color PCX image and that the palette may be designed by the
level author. It also documents gameplay meanings and reserved ranges for
palette indices. These statements describe converter input and game behavior;
they do not by themselves prove the on-disk `.LEV` layout.

One documentation range is written as `248-256`, although an 8-bit palette has
indices only from 0 through 255. The text is preserved as source evidence and
must not be silently translated into a parser rule.

## Next confirmation tests

1. Determine whether the freeware header difference represents a distinct
   format version or only different converter behavior.
2. Smoke-test classic writer output in the original V-Wing executable.
