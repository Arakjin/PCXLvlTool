# PXLP project format

`.pxlp` is PCX Level Tool's versioned, game-tagged editable project format.
It is not a game level file. A project must be published separately to the
selected game's `.LEV` format.

Version 1 stores:

- a stable game profile key
- tagged project settings, allowing new settings to be added later
- the shared 256-color RGB palette
- one or more named image documents such as `level` and `background`
- each document's dimensions and active layer
- up to five layers with names, visibility, locks, indexed pixels, and masks

Wings projects use `.pxlp` so the variable-size level, Wings gameplay settings,
and optional parallax document survive editing. Legacy V-Wing `.vwp` projects
remain supported separately.
