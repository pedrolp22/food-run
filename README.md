# Food Run

A C++20 lane runner starring an animated cat, with food collectibles, a day/night
cycle, configurable graphics and an optional PSX-inspired renderer.

## Download and play

Download the Windows or Linux x86_64 ZIP from [Releases](../../releases), extract
**the entire folder**, and run `foodrun.exe` on Windows or `./foodrun` on Linux.
Keep `assets/` beside the executable. No compiler, source checkout or separate SDL
installation is needed. Both platforms require a desktop display and an OpenGL
3.3-capable graphics driver. Official Linux builds target Ubuntu 22.04 / glibc 2.35
and newer compatible distributions.

Use **A/D** or **Left/Right** to change lanes and **Esc** to exit during play.
Menus support keyboard, mouse and controller input. Graphics settings save to
`game.cfg` beside the executable; extract the game into a writable folder.
See [player instructions](docs/PLAYING.md) and [rendering details](docs/rendering.md).

## Build from source

Requires CMake 3.20+, a C++20 compiler (GCC/Clang or MSVC/MinGW) and an OpenGL 3.3
driver. CMake uses installed SDL2 when available and otherwise downloads a pinned,
SHA-256-verified SDL2 release. GLAD and stb_image are included.

```sh
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=Release
cmake --build build/dev --config Release --parallel 2
ctest --test-dir build/dev -C Release --output-on-failure
./build/dev/foodrun
```

With Visual Studio's default generator, run `build/dev/Release/foodrun.exe`.
Development builds can find assets in the source tree, even when launched from
another working directory. Optional texture caches can be generated with
`cmake --build build/dev --config Release --target cook_textures`.

## Portable release ZIPs

The release configuration bundles static SDL and compiler runtime support and
loads assets from the executable's own folder. With Ninja and a configured compiler:

```sh
cmake --preset release
cmake --build --preset release --parallel 2
ctest --preset release
cpack --config build/release/CPackConfig.cmake -C Release -B dist
python3 scripts/verify_package.py dist
```

The ZIP and its SHA-256 checksum appear in `dist/`. GitHub Actions builds and
checks Windows and Linux packages; a `v<version>` tag also creates a draft release
with both platforms, checksums and matching GPL source. See the
[release guide](docs/releases.md) for Windows commands, cross-compilation, graphics
tests, minimum Linux compatibility and publication steps.

## Components

| CMake target | Responsibility | Runtime dependency |
| --- | --- | --- |
| `subway::math` | Matrices, vectors, normals, track curvature | None |
| `subway::assets` | CPU glTF import, validation, and sprite silhouette extrusion | None |
| `subway::graphics` | Scene commands, lighting, shadows, retro presentation, GPU resources | A current OpenGL context |
| `subway::platform` | SDL window and OpenGL context ownership | SDL2 |
| `subway::gameplay` | Game state, collisions, seeded simulation | None |
| `subway::world` | Food Run assets and procedural scene generation | Graphics resources |
| `subway::application` | Input, timing, window title, game renderer facade | All game components |

`include/Renderer.h` is the small game-facing facade. The reusable API is
`include/engine/graphics/SceneRenderer.h`; it accepts scene data and does not know about lanes, collectibles, or Food Run assets.
See [architecture and extension guide](docs/architecture.md) for ownership, rendering contracts, and migration notes.

## Tests and examples

CPU tests exercise math, simulation, assets, skeletal data and saved settings
without SDL or a display:

```sh
cmake --preset headless
cmake --build --preset headless --parallel 2
ctest --preset headless
```

Graphics tests require an OpenGL display. They cover rendering, materials,
reflections, retro effects, animation, menus and the application loop:

```sh
cmake -S . -B build/dev -DSUBWAY_ENABLE_GRAPHICS_TESTS=ON -DSUBWAY_BUILD_EXAMPLES=ON
cmake --build build/dev --config Release --parallel 2
ctest --test-dir build/dev -C Release --output-on-failure
./build/dev/renderer_example 3
```

The independent renderer example accepts an optional frame count for a bounded run.

## Reuse in another project

```cmake
set(SUBWAY_BUILD_GAME OFF CACHE BOOL "" FORCE)
set(SUBWAY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/subway)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE subway::graphics subway::platform)
```

Create a `Window`, then a `SceneRenderer`, populate a `Scene`, call `render(scene, window.drawableSize())`,
and call `window.swap()`. An existing OpenGL application can use `subway::graphics` with its own window/context layer;
set `SUBWAY_BUILD_PLATFORM=OFF` to avoid the SDL dependency entirely.
See [the complete example](examples/renderer_example.cpp).

## License and credits

Copyright (C) Food Run contributors. Our original code is free software: you can
redistribute it and/or modify it under the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version (**GPL-3.0-or-later**). It is distributed without any
warranty. See [LICENSE](LICENSE) for the full terms.

Third-party libraries and models retain their own licenses and attribution.
The bundled road barrier model is **CC-BY-NC-4.0**, so that asset is restricted to
noncommercial use. See [third-party notices](THIRD_PARTY_NOTICES.md). All model
license files are included in source and binary releases.

Game development: DedThred, Lorenzzo and rdhd, also credited in the in-game menu.
