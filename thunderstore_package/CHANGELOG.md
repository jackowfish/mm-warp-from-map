# Changelog

## 1.1.2
- Hide the hint and disable the warp toggle on the dungeon map page.
- Shrink the hint text so it stays clear of the MAP header.
- Replace the icon with an in-game screenshot.

## 1.1.1
- The on-screen hint now shows the C button you configured instead of always C-Down. (#1)
- Restyle the hint to match the game's own item-name text: the game's font, white with a
  navy outline. (#1)
- The hint's font is read from your own game at runtime; the mod ships no game assets.
- New icon.

## 1.0.5
- Fix confirming a destination doing nothing. Selecting a warp from the map now actually
  warps you, instead of just closing the menu.

## 1.0.4
- Nudge the on-screen prompt up by 5 pixels.

## 1.0.3
- Nudge the on-screen prompt down and to the right so it sits beside the MAP header.

## 1.0.2
- Move the on-screen prompt to the top-left of the map page (left of the MAP header)
  and draw it in black.
- Render the prompt from a higher-resolution, bold source texture so it stays sharp at
  high internal resolutions instead of looking blurry.

## 1.0.1
- Fix a crash on load. The mod now hooks the resident pause-menu wrappers
  (KaleidoScopeCall_Update / KaleidoScopeCall_Draw) instead of the relocatable
  kaleido overlay, and no longer reads overlay data that isn't mapped at load time.

## 1.0.0
- Initial release: switch the pause-menu map page into a warp selector (C-Down) to pick a Song of
  Soaring destination from the map. Only unlocked owl statues are selectable. Configurable button
  and on-screen prompt.
