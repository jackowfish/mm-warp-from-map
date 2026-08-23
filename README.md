# Warp Select

A mod for [Majora's Mask: Recompiled](https://github.com/Mr-Wiseguy/Zelda64Recomp) that lets you
pick a Song of Soaring destination from the pause-menu map instead of playing the song.

Open the pause menu, go to the map page, and press **C-Down** to switch into warp mode. Only owl
statues you've activated are selectable; move with the stick, **A** to warp, **C-Down/B** to switch
back, **Start** to close. Both the button and the on-screen prompt are configurable in the mod menu.

It works by flipping the map page into the game's own owl-warp selector (the one the Song of Soaring
uses), so the map, cursor, confirmation, and warp are all native.

## Screenshots

<img width="1512" height="949" alt="Screenshot 2026-08-23 at 4 17 20 PM" src="https://github.com/user-attachments/assets/3e7d2890-b742-4cd9-9935-1b0e63f8c450" />

<img width="1512" height="949" alt="Screenshot 2026-08-23 at 4 17 36 PM" src="https://github.com/user-attachments/assets/94f8d6c2-d220-4590-8197-7d0d20aac481" />

## Build

Needs LLVM `clang` + `ld.lld` (Apple clang won't work) and
[`RecompModTool`](https://github.com/N64Recomp/N64Recomp). Symlink or submodule `mm-decomp`
([zeldaret/mm](https://github.com/zeldaret/mm)) and `Zelda64RecompSyms`
([Zelda64Recomp/Zelda64RecompSyms](https://github.com/Zelda64Recomp/Zelda64RecompSyms)) at the repo
root, then run `./build.sh` to produce `build/warp_select.nrm`. Drag that onto the game window to
install.
