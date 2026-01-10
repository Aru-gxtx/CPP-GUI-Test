# CPP GUI Test

![CPP GUI Preview](CPP_GUI_prev.gif)

A **C++ Arcade Survival Game** built entirely using the **Dear ImGui** library.

I created this project as a learning exercise to better familiarize myself with the **Dear ImGui** immediate mode GUI library. It demonstrates how to utilize ImGui not just for tools and debug menus, but for rendering real-time 2D game graphics, handling game loops, and managing collision logic.



<p align="center">
  <a href="https://isocpp.org/">
    <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++">
  </a>
  <a href="https://github.com/ocornut/imgui">
    <img src="https://img.shields.io/badge/Dear%20ImGui-Library-fe7602?style=for-the-badge&logo=imgui&logoColor=white" alt="Dear ImGui">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/License-Educational-lightgrey?style=for-the-badge" alt="License">
  </a>
</p>

## Features

* **Infinite Difficulty Scaling:** Enemies spawn in waves. The longer you survive, the faster they spawn.
* **Stamina System:** Sprint mechanics with a visual stamina bar and overheat cooldown punishment.
* **Precise Hitboxes:** Dynamic circular collision detection that scales perfectly with the player's visual size.
* **Smooth Controls:** Full support for both **WASD** and **Arrow Keys**.
* **Game State Management:** Complete flow from Start Menu → Gameplay → Game Over screen with hotkey support.
* **Standalone:** No installation required; runs as a single portable executable.

## Installation

### For Players
1.  Go to the **[Releases](../../releases)** page.
2.  Download the latest `.exe` file.
3.  Double-click to play! (No Python or C++ installation needed).

### For Developers (Building from Source)
If you want to modify the code, you will need a C++ compiler (Visual Studio, MinGW, etc.) and the ImGui binaries.
1.  Clone this repository.
2.  Ensure you have `imgui`, `imgui_impl_win32`, and `imgui_impl_dx11` (or your backend of choice) linked.
3.  Compile `main.cpp`.

## Controls

| Action | Key 1 | Key 2 |
| :--- | :--- | :--- |
| **Move Up** | `W` | `Up Arrow` |
| **Move Down** | `S` | `Down Arrow` |
| **Move Left** | `A` | `Left Arrow` |
| **Move Right** | `D` | `Right Arrow` |
| **Sprint** | `LShift` | `RShift` |
| **Start / Retry** | `Enter` | `Space` |

## How It Works (Technical)

Since this was a learning project for ImGui, the architecture focuses on using `ImGui::GetWindowDrawList()` for rendering rather than a traditional sprite engine.

### 1. The Game Loop & State Machine
The application runs inside a standard ImGui frame loop. I implemented a simple state machine to handle the game flow:
* **State 0 (Menu):** Draws the title and "Start Game" button.
* **State 1 (Playing):** Handles the core logic (Player movement, Enemy Vector management, Collision checks).
* **State 2 (Game Over):** Displays the final score and high score, waiting for a reset signal.

### 2. Rendering
Instead of loading sprites into a game engine, this project uses:
* `ImGui::Image()` for the player character.
* `ImGui::GetWindowDrawList()->AddCircleFilled()` for drawing enemies efficiently.
* `ImGui::ProgressBar()` for the UI stamina bar.

### 3. Collision Logic
To ensure fairness, the collision system uses **Circle-to-Circle** intersection.
* **Player Hitbox:** Calculated dynamically as `Player_Width * 0.4` to ensure the hitbox centers on the character model regardless of the image size.
* **Enemy Hitbox:** Fixed radius to match the visual red circles.
* **Math:** Uses standard Euclidean distance formula $\sqrt{(x_2-x_1)^2 + (y_2-y_1)^2}$ to determine hits.

## Acknowledgments

* **[Ocornut](https://github.com/ocornut)** for creating the incredible Dear ImGui library.
* Built using C++ and DirectX11/Win32 (via ImGui backends).
