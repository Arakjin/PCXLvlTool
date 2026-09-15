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
```

`levdump` currently reports only directly observable byte-level properties:

- complete file size
- a hex/ASCII dump of the first 256 bytes
- printable ASCII strings at least four bytes long
- repeated runs of the same byte longer than 16 bytes

It does not identify or decode headers, dimensions, palettes, image data, or
compression.

## Observations

| Status | Offset or range | Observation | Evidence |
|--------|-----------------|-------------|----------|
| Unknown | — | No format structure has been established yet. | Initial project state. |

## Questions to investigate

- Is there a common header or signature?
- Does the format contain offsets or block lengths?
- Is the 640 x 800 indexed image stored as one stream or multiple blocks?
- Which compression scheme, if any, is used?
- Is the 256-color palette embedded in every file?
- Are there distinct classic and freeware format variants?
