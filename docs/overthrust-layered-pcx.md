# OverThrust Layered PCX (`.OPCX`) v1

OverThrust Layered PCX is a distinct format with the `.opcx` extension,
format ID `overthrust:layered-pcx-v1`, and media type
`application/vnd.overthrust.layered-pcx`. An OVL stores it in the `BACKGRND`
chunk.

OPCX keeps a conventional 8-bit, single-plane PCX image as its compatibility
preview. The preview is the exact composite of the visible embedded layers. A
versioned layer block is stored after the PCX image RLE stream and immediately
before the final `0x0c` 256-color palette marker. `materials.pcx` remains an
ordinary PCX without this extension.

All integers in the extension are unsigned 32-bit little-endian values. Text
is UTF-8. The extension has this layout:

```text
8 bytes   magic: "OPCXLAY1"
u32       payload byte length
u32       layer count (1..5)
u32       active layer index
repeat layer count times:
  u32     UTF-8 layer-name byte length (0..4096)
  bytes   layer name
  u8      flags: bit 0 visible, bit 1 locked
  3 bytes reserved, zero
  u32     encoded indexed-pixel stream byte length
  u32     encoded transparency-mask stream byte length
  bytes   indexed-pixel stream
  bytes   transparency-mask stream
```

Each stream encodes `height * bytes_per_line` bytes with PCX RLE, where
`bytes_per_line` is the even scanline length from the base PCX header. Layer
pixels are palette indices. Mask values are `0` for transparent and `1` for
opaque; padding bytes are zero. Every layer is encoded independently and in
bottom-to-top display order. Hidden layers remain separate and are not folded
into another layer.

The bottom layer is the opaque Background layer. On every higher layer,
palette index `0` is always transparent, even if its stored mask byte is `1`.
Writers canonicalize those mask bytes to `0`, and editors apply the rule while
drawing and pasting so empty source pixels do not need to be erased manually.
Nonzero pixels may also have a zero mask, allowing erased content to retain its
indexed value without becoming visible.

A reader that understands only ordinary PCX can use the flattened preview and
trailing palette. An OverThrust reader must validate the layer block and must
reject a file when the visible-layer composite differs from the preview.
