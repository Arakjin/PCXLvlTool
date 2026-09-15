# V-Wing `.LEV` reverse engineering

This document records observations about the V-Wing `.LEV` file format.
The original files are read-only reference material and must not be modified.

## Evidence rules

Every format claim must be classified as one of the following:

- **Confirmed:** demonstrated with multiple files or a controlled converter test.
- **Strongly suspected:** supported by evidence but not yet demonstrated.
- **Unknown:** observed data whose meaning has not been established.

Record how each claim was tested. Do not present an assumption as a fact.

## Reference corpus

The freeware levels `LEVEL1.LEV` through `LEVEL10.LEV` are stored outside this
repository in the parent V-Wing directory. They may be read for analysis but
must not be edited, moved, or copied into the repository.

## Tools

Build and run the initial inspection tool with:

```sh
cmake -S . -B build
cmake --build build
./build/levdump ../LEVEL1.LEV
./build/levcompare ../LEVEL1.LEV ../LEVEL2.LEV
./build/pcxfixtures build/oracle
```

`levdump` currently reports only directly observable byte-level properties:

- complete file size
- a hex/ASCII dump of the first 256 bytes
- printable ASCII strings at least four bytes long
- repeated runs of the same byte longer than 16 bytes

`levcompare` reports the input sizes, first differing byte, and contiguous
same/different ranges. If the files have different lengths, it also reports
the trailing range that exists in only one file. These range transitions are
byte-level comparison boundaries, not assumed format block boundaries.

`pcxfixtures` creates deterministic 640 x 800, 8-bit indexed PCX inputs for
controlled converter experiments. Its DOS-compatible filenames cover uniform
images, individual corner/ordering pixels, stripe and checkerboard patterns,
long runs, deterministic random pixels, and a palette-only change.

It does not identify or decode headers, dimensions, palettes, image data, or
compression.

Raw format observations are maintained in [`lev-format.md`](lev-format.md).

## Observations

| Status | Offset or range | Observation | Evidence |
|--------|-----------------|-------------|----------|
| Confirmed | Entire files | All ten reference files can be read by both GCC and Clang builds of `levdump`. | Corpus run on `LEVEL1.LEV` through `LEVEL10.LEV`. |
| Confirmed | Entire files | The corpus file sizes range from 55,939 to 150,591 bytes. | `levdump` file-size output. |
| Confirmed | Comparison | Different levels produce thousands of alternating equal/different byte ranges, so these transitions cannot by themselves be treated as format blocks. | `levcompare LEVEL1.LEV LEVEL2.LEV`. |
| Confirmed | `0x0080` through EOF | Converter 1.91 copies the PCX byte stream unchanged after replacing its 128-byte header. | Byte-for-byte comparisons of 15 controlled converter outputs. |
| Confirmed | EOF - 769 through EOF | A `0C` PCX palette marker and 768-byte RGB palette terminate every reference level. | Corpus inspection and controlled palette-change test. |

## Questions to investigate

- Is there a common header or signature?
- Does the format contain offsets or block lengths?
- Is the 640 x 800 indexed image stored as one stream or multiple blocks?
- Which compression scheme, if any, is used?
- Is the 256-color palette embedded in every file?
- Are there distinct classic and freeware format variants?
