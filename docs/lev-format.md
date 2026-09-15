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
| `0x0000` | 2 | Confirmed bytes, unknown meaning | Every corpus file begins with `76 07`. |
| `0x0002` | variable/unknown | Confirmed content, layout unknown | Printable uppercase text matching the displayed level names begins here in every corpus file. Shorter names are followed by spaces. The field boundary and termination rules are not yet established. |
| EOF - 768 | 768 | Strongly suspected | Candidate 256-entry RGB palette. The length is exactly `256 * 3`, the bytes form color-like triplets, and the converter documentation says the 256-color input palette is user-defined. Controlled converter tests are still required. |
| other | variable | Unknown | No header fields, image-data boundary, compression method, or offset table has been confirmed. |

The `LEVEL3.LEV` name area differs from the other nine samples around offsets
`0x0015` through `0x0018`. This is recorded as an anomaly only; it is not yet
evidence for a variable-length field.

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

Use the original converter to produce controlled files from PCX fixtures:

1. uniform images using palette indices 0, 1, and 57
2. one changed pixel at each image corner and at `(1, 0)`
3. horizontal stripes, vertical stripes, checkerboard, long runs, and random
   pixels
4. identical pixel data with one palette entry changed
5. identical image and palette with only the level name changed

Compare each output byte-for-byte. These tests should establish name layout,
pixel order, compressed-data boundaries, compression, and palette storage.
