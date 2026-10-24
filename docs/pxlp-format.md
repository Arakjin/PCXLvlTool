# PXLP project format

`.pxlp` is PCX Level Tool's versioned, game-tagged editable project format.
It is not a game level file. A project must be published separately to the
selected game's runtime format (`.LEV` or a single-file OverThrust `.OVL` package).

Version 1 stores:

- a stable game profile key
- tagged project settings, allowing new settings to be added later
- the shared 256-color RGB palette
- one or more named image documents such as `level` and `background`
- each document's dimensions and active layer
- up to five layers with names, visibility, locks, indexed pixels, and masks

For every editable document, the bottom Background layer is opaque. Palette
index `0` is always transparent on every higher layer. PXLP writers canonicalize
the corresponding mask bytes to zero, so copied empty pixels reveal the layers
below consistently in V-Wing, Wings, AUTS, and OverThrust projects. Publishing
still flattens the visible result into the target game's runtime format.

V-Wing, Wings, and AUTS projects use `.pxlp`. Wings' variable-size level,
gameplay settings, and optional parallax document, as well as AUTS' fixed-size
indexed image, therefore survive editing in the same extensible container.

Version 2 introduced OverThrust projects and remains readable. It stores one
indexed document with the `materials` role plus bounded typed sections for:

- an optional normalized sRGB, straight-alpha RGBA8 background, its parallax
  flag, decoded dimensions, SHA-256 content hash, and optional source filename;
- level identity and typed profile settings;
- declared activation channels;
- sliding doors, teleporters, service orbs, capture flags, and team bases with
  a shared world-pixel transform; team bases additionally retain an absolute
  world-pixel polygon defining their gameplay area;
- team-mode base bindings; and
- tagged unknown object properties retained for forward compatibility.

Unknown object types or properties survive editable-project round trips, but
they prevent publication under the active OverThrust authoring profile. Background source
paths are not needed after import: saving and publishing use the embedded
normalized pixels.

Version 3 is written for current OverThrust projects. It adds an always-present
`background` indexed document with exactly the same pixel dimensions as
`materials`. Its complete 256-color RGB palette is serialized separately, so
editing background colors never changes material preview colors. The indexed
background is distinct from the optional embedded RGBA presentation background:
it is fixed to world bounds and has no parallax or scroll settings.
