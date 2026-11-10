# Linkpulse

Linkpulse is a colorful, browser-inspired **offline GBA homebrew game**. It simulates a tiny local web: browse fictional sites, create a site in the current play session, discover an easter egg, and play a built-in mini-game.

It is a game, not a real web browser. GBA software running in Linkboy cannot load live websites, run desktop browser extensions, or access arbitrary files dragged from a computer. User-created sites in this first version are held in memory and reset when the game is restarted.

## Build

Install devkitPro with the devkitARM toolchain and libgba, then run:

```sh
make
```

This produces `linkpulse.gba` in the repository root. To remove build outputs:

```sh
make clean
```

The ROM can then be opened in Linkboy or another GBA emulator. Linkboy availability and ROM compatibility depend on the app/device version; this project does not include copyrighted game ROMs or BIOS files.

## Controls

- **D-pad:** navigate menus and edit the site title / move in the mini-game
- **A:** select, open, or collect
- **B:** go back
- **START:** publish a user-made local site or start/restart the mini-game

## Current prototype

- 240×160 GBA display, rendered with libgba
- Linkpulse home screen and local site directory/search
- Site viewer for built-in fictional pages
- In-game title editor and publish flow for one temporary user-made site
- Built-in collect-the-star mini-game
- Hidden button-sequence easter egg

## Project layout

- `source/main.c` — rendering, navigation, local sites, editor, game, and easter egg
- `Makefile` — devkitARM/libgba build
