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

### Building

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/Minecraft.git
    cd Minecraft
    ```

2. Build the project:
    ```sh
    gcc -o nob nob.c
    ```
    Then run nob

3. From your explorer or different terminals
   Launch the build/server.exe
   Launch the build/game.exe
    

## Controls

- `W`, `A`, `S`, `D`: Move the player
- Mouse: Look around
- `Left Shift`: Sprint

## Acknowledgements

- [Raylib](https://www.raylib.com/) for the graphics library
