# Minecraft

This project is a simple Minecraft-like game written in C using the Raylib library for rendering and a custom network library for multiplayer support.

## Features

- Basic terrain generation with chunks
- First-person camera controls
- Multiplayer support with player state synchronization
- Simple network server to handle player connections and state updates

## Getting Started

### Prerequisites

- Raylib library
- A C compiler (e.g., cc)

### Building (cross-platform)

This repo now includes a `Makefile` and small helper scripts to build on macOS and Windows (MSYS2/MinGW).

macOS (Homebrew):

1. Install prerequisites if needed:

```sh
brew install raylib pkg-config
```

2. Build:

```sh
./build.sh
# or
make
```

The resulting binary will be `./game` (run with `./game`).

Windows (MSYS2 / MinGW):

1. Install MSYS2 from https://www.msys2.org and open the MinGW64 shell.

2. Install toolchain and raylib:

```sh
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-raylib mingw-w64-x86_64-pkg-config
```

3. Build inside the MinGW64 shell:

```sh
make
# or from Windows cmd if configured: build_windows.bat
```

The resulting binary will be `game.exe`.

Notes:
- The `Makefile` prefers `pkg-config` to discover raylib flags. If `pkg-config` is not available it falls back to Homebrew paths on macOS or `./lib` on Windows (legacy behaviour).
- If you previously used the custom `nob` builder (see `nob.c`), it linked against a raylib in `./lib` and added Windows libraries (`-lopengl32 -lgdi32 -lwinmm -lws2_32`). The new `Makefile` handles these cases when `pkg-config` is unavailable.

If you want, I can also:

- Add a `target` in the `Makefile` for a `server` binary (if `src/server.c` is present).
- Add a VSCode `tasks.json` to build from the editor.


## Controls

- `W`, `A`, `S`, `D`: Move the player
- Mouse: Look around
- `Left Shift`: Sprint

## Acknowledgements

- [Raylib](https://www.raylib.com/) for the graphics library
