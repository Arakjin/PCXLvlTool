# Changelog

All notable changes to PCX Level Tool are documented in this file.

## 0.2.0 - Unreleased

Planned first multi-game development release.

- Add Wings as the second supported game, including variable-size levels,
  gameplay settings, an optional parallax-background document, and native
  Wings `.LEV` publishing.
- Replace the V-Wing-specific editable project path with the shared,
  versioned, game-tagged `.pxlp` format.
- Introduce game profiles and feature flags so additional indexed-PCX games
  can be added without duplicating the common editor backend.
- Preserve the existing V-Wing editing and classic `.LEV` publishing workflow.
- Keep local Wings executables, game data, and research references outside Git
  and release packages.

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
