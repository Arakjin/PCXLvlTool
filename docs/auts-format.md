# AUTS level and bitmap format

This document records the behavior verified against the original AUTSCONV
utilities. The local `AUTSCONV/` directory is research material and is not
tracked or distributed with PCX Level Tool.

## Confirmed image model

- The level is a 320 x 400, 8-bit indexed image.
- The fixed 256-color RGB palette is stored in `BLANK.BMP`, not in `.LEV`.
- BMP input is an uncompressed Windows 3.x-style 8-bit indexed bitmap.
- BMP rows are stored bottom-up; decoded LEV pixels are ordered top-down.
- BMP2LEV replaces the outer two pixels on every side with index 7.

The documented special indices are:

| Index | Meaning |
|---:|---|
| 0 | Space |
| 7 | Indestructible |
| 39 | Water |
| 92–95 | Docking plate |

Indices 92–95 should not be mixed with the visually similar indices 108–111.

## Confirmed LEV RLE

The LEV pixel stream has no header. A literal value is stored as one byte. A
run of 2–256 identical pixels is stored as:

```text
value, value, run_length - 1
```

Longer runs are split into chunks of at most 256 pixels. BMP2LEV also ends a
run at the boundary between the upper and lower 200-row halves, matching the
two-part processing and preview in the original tool. Decoding stops after
exactly 128,000 pixels.

Original BMP2LEV outputs contain eight bytes after the pixel stream. Their
values changed when the same decoded bitmap was converted in a different DOS
process, while every byte of the actual RLE stream remained identical. This is
consistent with an eight-byte over-read or uninitialized converter trailer,
not level metadata. PCX Level Tool writes eight deterministic zero bytes in
the same location; readers ignore their values but require the compatible
eight-byte layout. The original LEV2BMP successfully decoded a PCX Level Tool
file with this zeroed trailer.

The codec was verified by decoding the supplied `SANDIS.LEV` and `SLIME.LEV`.
Their 128,000 decoded indices matched LEV2BMP output exactly. Re-encoding those
BMP files with BMP2LEV reproduced the complete RLE portion byte for byte; only
the final eight incidental bytes differed.

## Converter constraints

The original documentation states:

- at most seven water surfaces per level
- each water surface should be shorter than 100 pixels
- a water surface must either touch the ceiling or remain more than five
  pixels below it, so waves cannot reach the ceiling

These are currently documented editing rules, not automatic validation.

## Image import policy

PCX Level Tool accepts only the confirmed 320 x 400, uncompressed 8-bit indexed
BMP layout for AUTS. Pixel indices are preserved rather than inferred from RGB
colors, and the fixed AUTS palette is applied. A differing source palette is
reported to the user because material semantics depend on the indices.

AUTS also accepts an 8-bit indexed PCX of exactly the same dimensions. The
Import action always asks the user for the target game; game identity is not
guessed from dimensions or palette colors.
