# PXLP project format

`.pxlp` is PCX Level Tool's versioned, game-tagged editable project format.
It is not a game level file. A project must be published separately to the
selected game's runtime format (`.LEV` or an OverThrust `.OLV` package directory).

Version 1 stores:

- a stable game profile key
- tagged project settings, allowing new settings to be added later
- the shared 256-color RGB palette
- one or more named image documents such as `level` and `background`
- each document's dimensions and active layer
- up to five layers with names, visibility, locks, indexed pixels, and masks

V-Wing, Wings, and AUTS projects use `.pxlp`. Wings' variable-size level,
gameplay settings, and optional parallax document, as well as AUTS' fixed-size
indexed image, therefore survive editing in the same extensible container.

Version 2 is used by OverThrust projects and remains backward-readable with
version 1. It stores exactly one indexed document with the `materials` role,
plus bounded typed sections for:

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
they prevent publication under `overthrust:authoring-v4`. Background source
paths are not needed after import: saving and publishing use the embedded
normalized pixels.
