# VWP project format

`.vwp` is the editor's lossless layered project format. It is separate from the
game-compatible `.LEV` export. All integers are unsigned 32-bit little-endian
values and strings are a 32-bit byte count followed by UTF-8 bytes.

Version 1 is laid out as follows:

1. eight-byte magic `VWPROJ1\0`
2. format version (`1`), width (`640`), and height (`800`)
3. level-name string
4. 256 RGB triplets
5. layer count (1-5) and active-layer index
6. for every layer, from bottom to top:
   - name string
   - one flag byte: bit 0 is visible and bit 1 is locked
   - 640 x 800 palette-index bytes
   - 640 x 800 mask bytes (`0` transparent, nonzero opaque)

The first layer is always loaded as the visible, fully opaque `Background`.
Higher layers use only binary transparency. Unknown versions, invalid sizes,
invalid layer metadata, truncated data, and trailing data are rejected.

Publishing composites visible layers from top to bottom and writes the result
through the normal `.LEV` writer. Hidden layers remain in the project but do
not appear in the published level.
