# Changelog

All notable changes to PCX Level Tool are documented in this file.

## Unreleased

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
