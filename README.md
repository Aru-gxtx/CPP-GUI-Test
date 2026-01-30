# CPP GUI Tests

A collection of **C++ Games and Tech Demos** built entirely using the **Dear ImGui** library.

I created this repository as a learning exercise to master the **Dear ImGui** immediate mode GUI library. It demonstrates that ImGui can be used for more than just debug tools—it can handle real-time 2D graphics, 3D rendering, custom physics loops, and complex game state management.

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

---

## Download & Installation

Both projects are compiled and available for download.

1.  Go to the **[Releases](../../releases)** page.
2.  You will find **two results** (executables) available for download:
    * `GalleDodge` (The 2D Game)
    * `3DEngine` (The 3D Tech Demo)
3.  Download the one you wish to try and double-click to run! (No installation needed).

---

## Project 1: GalleDogde (2D)

![Galledodge Preview](README_assets/GalleDodge_prev.gif)

A fast-paced survival game where you dodge enemies and manage stamina.

### Features
* **Infinite Difficulty Scaling:** Enemies spawn in waves. The longer you survive, the faster they spawn.
* **Stamina System:** Sprint mechanics with a visual stamina bar and overheat cooldown punishment.
* **Precise Hitboxes:** Dynamic circular collision detection that scales perfectly with the player's visual size.
* **Game State Management:** Complete flow from Start Menu → Gameplay → Game Over screen.

### Controls
| Action | Key 1 | Key 2 |
| :--- | :--- | :--- |
| **Move** | `WASD` | `Arrow Keys` |
| **Sprint** | `LShift` | `RShift` |
| **Start / Retry** | `Enter` | `Space` |

---

## Project 2: 3DEngine (3D)

![3D Engine Preview](README_assets/3DEngine_prev.mp4)

A "Minecraft-style" 3D rendering and physics engine running inside an ImGui window. This project focuses on complex 3D math and collision logic.

### Features
* **3D Camera System:** Full First-Person mouse look (Yaw/Pitch) and movement.
* **Advanced Physics:** Gravity acceleration, jumping mechanics, and velocity-based movement.
* **Voxel Collision:**
    * **AABB Collision:** Prevents clipping through blocks.
    * **Wall Sliding:** Smart axis-separation allowing players to slide along walls rather than getting stuck.
    * **Step Logic:** Automatically detects if a block is low enough to step on or requires a jump.
* **Terrain Generation:** Renders a voxel-based world grid.

### Controls
| Action | Key |
| :--- | :--- |
| **Move** | `WASD` |
| **Look** | `Mouse` |
| **Jump** | `Space` |

---

## Technical Details (How It Works)

Since this was a learning project, the architecture focuses on pushing `ImGui::GetWindowDrawList()` to its limits.

### 1. Rendering Strategy
* **2D Mode:** Uses `AddCircleFilled()` for enemies and `ImGui::Image()` for the player.
* **3D Mode:** Projects 3D world coordinates into 2D screen space using a custom Camera Matrix (Perspective Projection), then draws quads using the DrawList API to simulate 3D blocks.

### 2. Collision Logic
* **2D Collision:** Uses Euclidean distance $\sqrt{(x_2-x_1)^2 + (y_2-y_1)^2}$ for Circle-to-Circle checks.
* **3D Collision:** Implements strictly defined bounding boxes. It checks "future positions" (velocity integration) against the terrain heightmap to determine if a move is valid, if the player should slide, or if gravity should apply.

## Acknowledgments

* **[Ocornut](https://github.com/ocornut)** for creating the incredible Dear ImGui library.
* Built using C++ and DirectX11/Win32 (via ImGui backends).
