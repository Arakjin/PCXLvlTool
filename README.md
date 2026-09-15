# V-Wing Level Editor

Portable C++17 and Qt 6 tools for inspecting and editing V-Wing `.LEV` files
and layered `.vwp` editor projects.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The only required dependencies are Qt 6 Widgets and SVG, a C++17 compiler,
CMake, and the C++ standard library.

## Minimal editor

Run:

```sh
./build/VWingLevelEditor
```

The current milestone supports:

- opening `.LEV` levels and layered `.vwp` projects
- creating a fresh level with **File > New level** (`Ctrl+N`)
- editing the in-game level-menu name directly in the top toolbar; LEV names
  are limited to 20 printable ASCII characters, so non-ASCII letters such as
  `ä`, `ö`, and `å` are blocked, and the name is forced to uppercase when saved
- saving editable work as `.vwp`, plus a separate **Publish LEV** action that
  flattens the visible layers into a game-compatible `.LEV` file
- up to five reorderable, binary visible/hidden layers; transparent pixels in
  upper layers reveal the layers below
- a fixed, always-visible Background layer at the bottom; its eraser writes
  Color Chart 1 (file/palette index 0), while erasing upper layers makes them
  transparent
- layer duplication, naming, locking, deletion, and top-to-bottom ordering in a
  dedicated Layers panel
- pixel-perfect indexed rendering at 25%, 50%, 100%, 200%, 400%, and 800%,
  with `Ctrl+mouse wheel` zooming around the pointer
- panning with the middle mouse button
- Paint-style horizontal toolbox with pencil, eraser, line, cubic Bezier
  curve, rectangle, ellipse, polygon, text, spray, flood-fill, eyedropper, and
  selection tools, plus a separate horizontal options bar
- independently remembered 1-32 pixel thickness for pencil, eraser, line,
  rectangle, ellipse, polygon, spray, and Bezier curve
- sparse spray dragging with distance-based spacing, while a single click keeps
  the full spray dab
- live, non-destructive previews while dragging line and shape tools
- Paint-style Bezier workflow: drag a baseline and then its two bend points;
  the completed curve is committed as one undo operation
- `Shift`-constrained squares and circles, plus selectable sharp or rounded
  rectangle corners; rectangle, ellipse, and polygon support outline-only,
  outline-and-fill, and fill-only modes using the primary index for outlines
  and the secondary index for interiors
- movable, non-destructive indexed text boxes with direct canvas typing,
  selectable font family, and 6-64 pixel size; `Ctrl+Enter` or clicking outside
  accepts and `Esc` cancels
- Paint-style polygons made by clicking corners and completed with a double
  click or `Enter`; `Esc` cancels an unfinished polygon
- rectangular, elliptical, and freehand selections stored as temporary indexed
  layers; selections can be moved, deleted, or edited with the drawing tools
- lossless indexed selection copy/paste with `Ctrl+C` and `Ctrl+V`; `Enter`
  commits a floating selection, `Esc` cancels it, and `Ctrl+A` selects the full
  640x800 canvas
- stroke-based undo and redo (`Ctrl+Z` and `Ctrl+Shift+Z`)
- a clickable 256-color palette viewer with exact index, RGB, hexadecimal, and
  documented V-Wing material information; left click selects the primary index
  and right click selects the secondary index, which also works for drawing
- palette-area filtering that omits reserved and "do not use" indices;
  reserved Color Chart indices are also blocked in direct material selection
- reserved default colors derived per index from the most common RGB values in
  the original `LEVEL1.LEV` through `LEVEL11.LEV` palettes
- per-index RGB editing with undo, plus reusable 256-color JASC-PAL load/save
- cursor coordinates and the current pixel index in the status bar
- prompts before discarding unsaved edits

Published LEV files always use the converter 1.91-compatible classic header.
The `.vwp` format retains layer pixels, transparency, order, visibility, locks,
names, palette, and the active layer. Keep a backup of levels used for testing.
The original reference files in the parent V-Wing directory are read-only
research material and must not be modified.

Untitled levels start with an editor-designed material palette that visually
separates water, explosives, normal terrain, burnable terrain, underwater
materials, indestructible terrain, and turret parts. It is a practical starter
palette, not a claim about one canonical V-Wing palette. Opening a `.LEV` file
always uses that file's embedded palette unchanged.

The converter's Color Chart uses human-facing numbers 1-256, while PCX and LEV
store palette indices 0-255. The editor's color selectors show Color Chart
numbers and the details panel also reports the corresponding file index.

## Command-line tools

- `levdump`: raw inspection plus strict LEV decode status
- `levcompare`: byte-range comparison
- `lev2pcx`: validated LEV to indexed PCX export
- `levroundtrip`: LEV load/save and optional internal-name replacement
- `pcxfixtures`: deterministic PCX corpus for converter research

Format evidence and research status are documented in
[`docs/lev-format.md`](docs/lev-format.md) and
[`docs/reverse-engineering.md`](docs/reverse-engineering.md). The editable
project container is documented in [`docs/vwp-format.md`](docs/vwp-format.md).

## Third-party artwork

Most toolbox icons come from
[KolourPaint](https://invent.kde.org/graphics/kolourpaint) and are used under
the BSD 2-Clause License. Source and license details are in
[`third_party/kolourpaint/README.md`](third_party/kolourpaint/README.md).

The move-selection icon comes from
[Tabler Icons v3.46.0](https://github.com/tabler/tabler-icons/tree/v3.46.0)
and is used under the MIT License. The bundled license is in
[`third_party/tabler-icons/LICENSE`](third_party/tabler-icons/LICENSE).
