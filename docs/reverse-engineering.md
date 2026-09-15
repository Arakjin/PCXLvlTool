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
./build/lev_tests build/oracle
./build/lev2pcx ../LEVEL1.LEV build/LEVEL1.PCX
./build/levroundtrip ../LEVEL1.LEV build/LEVEL1.LEV
./build/levroundtrip ../LEVEL1.LEV build/LEVEL11.LEV "TEST"
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

`lev2pcx` decodes a validated LEV into the internal `Level` model and writes a
standard 640 x 800, 8-bit indexed PCX while preserving every palette index and
RGB palette entry.

`levroundtrip` exercises the LEV writer by loading a source level and writing a
new classic-format LEV. An optional third argument overrides the internal level
name. The writer rejects names longer than 20 bytes and non-printable ASCII
instead of silently truncating or converting them.

The raw inspection sections do not assign format meanings to bytes. After
those sections were implemented, controlled converter tests established the
PCX-derived layout, and `levdump` was extended to report whether the strict LEV
reader can decode the file as a 640 x 800 indexed image plus palette.

Raw format observations are maintained in [`lev-format.md`](lev-format.md).

## Observations

| Status | Offset or range | Observation | Evidence |
|--------|-----------------|-------------|----------|
| Confirmed | Entire files | All ten reference files can be read by both GCC and Clang builds of `levdump`. | Corpus run on `LEVEL1.LEV` through `LEVEL10.LEV`. |
| Confirmed | Entire files | The corpus file sizes range from 55,939 to 150,591 bytes. | `levdump` file-size output. |
| Confirmed | Comparison | Different levels produce thousands of alternating equal/different byte ranges, so these transitions cannot by themselves be treated as format blocks. | `levcompare LEVEL1.LEV LEVEL2.LEV`. |
| Confirmed | `0x0080` through EOF | Converter 1.91 copies the PCX byte stream unchanged after replacing its 128-byte header. | Byte-for-byte comparisons of 15 controlled converter outputs. |
| Confirmed | EOF - 769 through EOF | A `0C` PCX palette marker and 768-byte RGB palette terminate every reference level. | Corpus inspection and controlled palette-change test. |
| Confirmed | Decoded data | All ten reference levels decode to exactly 512,000 palette indices and 256 RGB entries without crossing scanline boundaries. | Strict reader corpus run. |
| Confirmed | Pixel order | Controlled corner pixels, stripes, checkerboard, long runs, and deterministic random pixels decode byte-for-byte to the generated source indices. | `lev_tests build/oracle`. |

## Milestone status

- Milestone 1, format research: complete for the data stream, compression, and
  palette; some header fields remain intentionally unknown.
- Milestone 2, LEV reader: implemented and validated against the ten-file
  reference corpus and controlled converter outputs.
- Milestone 3, PCX export: implemented. All ten reference levels export to PCX
  files accepted by an independent decoder. All 15 controlled pixel/palette
  fixtures round-trip from LEV to PCX byte-for-byte identically to their source
  PCX files.
- Milestone 4, LEV writer: implementation and binary converter-oracle checks
  complete. All 20 controlled cases are byte-for-byte identical to converter
  1.91 output, and all ten reference levels preserve decoded content across a
  round trip. A writer-generated `LEVEL11.LEV`, renamed internally to `TEST`,
  loaded and played correctly in the original game.
- Milestone 5, minimal GUI: implemented and build-tested with Qt 6 using both
  GCC and Clang. Open, Save, Save As, indexed canvas rendering, fixed zoom
  levels, middle-button pan, pencil drawing, material-index selection, cursor
  status, and unsaved-change prompts are present. Interactive testing confirmed
  that levels open, can be drawn on and saved, and the GUI-saved output opens
  correctly in the original game.
- Milestone 6, usable editor: in progress. Stroke-based undo/redo and the
  Paint-style pencil, eraser, line, outline/filled rectangle, outline/filled
  ellipse, flood-fill, and eyedropper tools are implemented. Shape tools have
  a live preview. The palette viewer exposes all 256 exact indices, RGB values,
  and converter-documented material meanings. It can filter to documented,
  usable Color Chart groups without reserved indices, while direct index input
  rejects the chart's reserved entries. Individual RGB entries and complete
  JASC-PAL files can be edited, loaded, and saved; palette changes use the same
  undo history as pixel edits. Untitled levels receive an explicitly
  editor-designed starter palette; its reserved entries use the per-index RGB
  modes measured from the original `LEVEL1.LEV` through `LEVEL11.LEV` files.
  Pencil, eraser, and line thicknesses are
  independently configurable, as are rectangle and ellipse outline widths.
  Rectangle corners have selectable radii, and Shift constrains rectangles and
  ellipses to squares and circles. Spray painting and rectangular, elliptical,
  and freehand floating selections are implemented without changing the indexed
  level model. Selection movement, masked drawing, deletion, and clipboard
  round-trips are undoable when committed. Cubic Bezier curves use a baseline
  followed by two interactive control-point drags and commit as one pixel edit.
  Canvas and palette interaction paths are covered by offscreen tests.

## Questions to investigate

- What are the meanings of the still-unknown constant header bytes?
- Does the one-byte freeware header shift identify a distinct format version
  or only a newer converter implementation?
