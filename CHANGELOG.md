# Changelog

All notable changes to PCX Level Tool are documented in this file.

## Unreleased

- Add an active-layer magic-wand selection tool with connected-area and
  all-matching-material modes.
- Add the OverThrust authoring profile with its material registry,
  true-color background import and preview, typed settings, and non-rasterized
  sliding gates, teleporters, service orbs, and team bases.
- Add dedicated OverThrust object-tool icons and canvas sprites, independently
  sized sliding-gate previews, editable teleporter targets, automatic pairing for new
  unpaired teleporters, and dotted connection overlays.
- Define team-base gameplay coverage with editable canvas polygons, retain the
  polygon in PXLP, and publish it in deterministic objects schema version 4.
- Separate capture flags from team-base polygons so each flag has its own
  draggable object position and explicit CTF team binding in the manifest.
- Deselect a newly created team base automatically, preserve an empty object
  selection across list refreshes, and make Escape reliably cancel polygon
  creation while discarding its vertices.
- Expand rock, bedrock, soil, and rubble appearance indices; expose indices
  97–255 as empty fly-through colors; and rename toxic gas to acidic gas.
- Keep the imported presentation PNG hidden during editing by default, with an
  explicitly labelled optional canvas-preview toggle.
- Add direct object move, separate width/height resize, corner resize, and
  rotation handles while retaining numeric properties for precision.
- Publish deterministic `.OLV` level-package directories containing canonical
  `manifest.json`, `materials.pcx`, and `objects.json` files, plus
  `background.png` when an optional presentation background is supplied.
- Simplify OverThrust settings by removing level player-count, spawn-policy,
  and boundary-policy fields; author world dimensions and object measurements
  directly in pixels without editor-side cell conversion; and add a background
  parallax flag alongside the existing scroll factors.
- Add PXLP v2 persistence with a backward-readable OverThrust data v4 section
  for optional embedded backgrounds, settings, and forward-compatible object
  records while retaining older project loading.
- Replace separate OverThrust level ID and display-name inputs with one level
  name used for both, show the capture-the-flag player count only when that
  mode is enabled, and add a labelled team-base polygon button.
- Allow indexed PCX files to be opened directly under a selected compatible
  game profile and export the current flattened document as PCX.
- Correct the PCX writer for variable and odd-sized images with deterministic
  row padding and bounded RLE runs.
- Keep local OverThrust contract documents and golden fixtures out of Git.

## 0.2.5 - 2026-09-17

- Add user-level, game-specific default directories for publishing V-Wing,
  Wings, and AUTS `.LEV` files.
- Add a configurable `.pxlp` project directory with a Documents-based default.
- Add in-application user-guide and license windows, with separate V-Wing,
  Wings, and AUTS guides under a Game guides submenu.
- Correct V-Wing palette groups to include the bird color and distinguish
  shared game-effect colors from material placement.
- Disable palette-color editing for fixed game colors while keeping usable
  material indices available for drawing.
- Keep the pencil and eraser footprint preview under the cursor while dragging
  over pixels that already contain the requested result.
- Show the internal Level name field only for V-Wing; Wings and AUTS derive
  their level names from the published filenames.
- Clarify in the user documentation that each game's `.LEV` format is
  incompatible with the others, and move shared editor behavior out of the
  game-specific guides.
- Keep both Bezier control handles editable until the curve is accepted with
  Enter, and allow Escape to cancel it.
- Prevent a shutdown crash when discarding an unsaved document with a dirty
  undo stack.

## 0.2.0 - 2026-09-16

First multi-game release.

- Add Wings as the second supported game, including variable-size levels,
  gameplay settings, an optional parallax-background document, and native
  Wings `.LEV` publishing.
- Add AUTS level creation, its fixed 320 x 400 palette/material profile,
  indexed BMP and PCX import, existing-level loading, and native AUTS `.LEV`
  publishing.
- Add a single game-explicit image import flow: choose V-Wing, Wings, or AUTS,
  then import an image validated against that profile.
- Replace the V-Wing-specific editable project path with the shared,
  versioned, game-tagged `.pxlp` format.
- Introduce game profiles and feature flags so additional indexed-PCX games
  can be added without duplicating the common editor backend.
- Preserve the existing V-Wing editing and classic `.LEV` publishing workflow.
- Keep local Wings and AUTS converter executables, game data, and research
  references outside Git and release packages.

## 0.1.0 - 2026-09-15

First public test release.

- Open, edit, and publish V-Wing `.LEV` levels.
- Save layered `.vwp` editing projects with up to five layers.
- Pixel-accurate Paint-style drawing, shape, text, fill, palette, and selection
  tools.
- Primary and secondary material indices with documented V-Wing material
  groups and reserved-index protection.
- Custom indexed palettes and JASC-PAL import/export.
- Portable Windows x86_64 ZIP and Linux x86_64 AppImage release targets.
- Original V-Wing Level Converter 1.91 retained with its unmodified FreeWare
  documentation.
