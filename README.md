# PGproject(Evening Park)

A small C++ OpenGL demo showcasing an night(evening) park scene with dynamic lighting, shadow mapping, animated models, weather effects and basic collision handling. The application is implemented using modern OpenGL (core profile) and is intended to run from Visual Studio 2022 (Windows) or other platforms with an OpenGL 4.x driver.

## Features

- Forward renderer with per-object material support
- Shadow mapping using a "moon" light (directional/orthographic shadow map)
- Textured ground and skybox
- Multiple imported 3D models (benches, street lamps, trees, statue, themed truck, moon)
- Simple animations: lamp flicker, tree sway, bobbing/rotating props
- Toggleable weather effects: fog and a GPU-driven rain particle system
- Multiple render modes: solid, wireframe, points, smooth shading
- Camera with collision detection using axis-aligned bounding boxes (AABB)
- Runtime controls for lights, rendering modes and weather

## Controls

- Movement: `W`, `A`, `S`, `D`
- Vertical movement: `Q` (up), `E` (down)
- Mouse: look around (cursor is captured)
- Scroll wheel: change field of view
- Toggle fog: `0`
- Toggle rain: `9`
- Toggle collision detection: `B`
- Moon rotation: `1` (left), `2` (right)
- Render modes:
  - `Z` — Solid
  - `X` — Wireframe
  - `C` — Points
  - `V` — Smooth
- Exit: `Esc`

## Project layout

- `main.cpp` — application entry point, scene setup and render loop
- `include/` — utility classes (`Shader`, `Camera`, `Model`, `Skybox`, `TextureLoader`, etc.)
- `shaders/` — GLSL shader programs (`basic`, `shadow`, `rain`, ...)
- `models/` — OBJ models and materials
- `textures/` — textures used by models and terrain

## Dependencies

- OpenGL 4.1+ (core profile)
- GLFW (windowing and input)
- GLEW (or alternative loader; GLEW is used on non-macOS builds)
- GLM (math library)
- A simple image loader (for example, `stb_image.h`) used by `TextureLoader`

Use package managers (vcpkg, Conan) or add libraries manually to your Visual Studio project. On macOS the code uses native core profile includes and will skip GLEW.

## Build & Run (Visual Studio 2022)

1. Ensure dependencies are installed and their include/lib paths configured.
2. Open or create a Visual C++ project and add the source files and the `include/` headers.
3. Make sure the working directory (or executable's runtime working folder) contains the `models/`, `textures/` and `shaders/` folders so assets can be loaded at runtime.
4. Build and run.

If an asset fails to load the application prints a warning to the console.

## Assets & Licensing

This project uses external 3D models and textures.

## Screenshots

- Full scene:
  
  <img width="1270" height="709" alt="Screenshot 2026-02-20 221214" src="https://github.com/user-attachments/assets/d51ff22f-377d-4a44-bbc3-aa64a65d955e" />

- Wireframe / points:

  <img width="1262" height="707" alt="Screenshot 2026-02-20 221247" src="https://github.com/user-attachments/assets/899a2cb3-e77e-493f-ab93-0f44f35a0636" />
  <img width="1269" height="708" alt="Screenshot 2026-02-20 221301" src="https://github.com/user-attachments/assets/6e57fc76-28ba-41d9-bf8d-a20a98c2d599" />
  
- Rain:

  <img width="1266" height="709" alt="Screenshot 2026-02-20 221400" src="https://github.com/user-attachments/assets/c393fdb4-0c64-4020-96d3-901a817ed0fa" />
  
- Fog:

  <img width="1254" height="704" alt="Screenshot 2026-02-20 221512" src="https://github.com/user-attachments/assets/b3471b79-a848-49c4-831a-baed13b6a428" />


## Development notes

- The camera uses AABB colliders to prevent leaving the playable area; collider definitions live in `main.cpp` and can be tuned.
- Shadow map resolution can be adjusted in `main.cpp` (`SHADOW_WIDTH`, `SHADOW_HEIGHT`).
- Rain particle count is defined by `MAX_RAIN_PARTICLES` and can be adjusted for performance.
