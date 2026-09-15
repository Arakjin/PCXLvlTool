# V-Wing Level Editor

Portable C++17 and Qt 6 tools for inspecting and editing V-Wing `.LEV` files.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The only required dependencies are Qt 6 Widgets, a C++17 compiler, CMake, and
the C++ standard library.

## Minimal editor

Run:

```sh
./build/VWingLevelEditor
```

The current milestone supports:

- opening, saving, and saving `.LEV` files under a new name
- pixel-perfect indexed rendering at 25%, 50%, 100%, 200%, 400%, and 800%
- panning with the middle mouse button
- pencil drawing with the left mouse button
- stroke-based undo and redo (`Ctrl+Z` and `Ctrl+Shift+Z`)
- selecting an exact palette index from 0 through 255
- cursor coordinates and the current pixel index in the status bar
- prompts before discarding unsaved edits

The editor always writes the converter 1.91-compatible classic header. Keep a
backup of levels used for testing. The original reference files in the parent
V-Wing directory are read-only research material and must not be modified.

## Command-line tools

- `levdump`: raw inspection plus strict LEV decode status
- `levcompare`: byte-range comparison
- `lev2pcx`: validated LEV to indexed PCX export
- `levroundtrip`: LEV load/save and optional internal-name replacement
- `pcxfixtures`: deterministic PCX corpus for converter research

Format evidence and research status are documented in
[`docs/lev-format.md`](docs/lev-format.md) and
[`docs/reverse-engineering.md`](docs/reverse-engineering.md).
