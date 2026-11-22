Food Run
========

Extract the entire ZIP into a writable folder. Keep the assets folder beside the
executable. No source code, compiler, SDL installation or installer is needed.

Windows (64-bit): open foodrun.exe.
Linux (64-bit): run ./foodrun. If your archive tool removes executable permission,
run chmod +x foodrun first. Official Linux builds target Ubuntu 22.04 and newer
compatible distributions with glibc 2.35 or newer.

A desktop display and an OpenGL 3.3-capable graphics driver are required on both
platforms. Install graphics drivers through your operating system or GPU vendor.

Controls
--------
A/D or Left/Right: change lanes.
Esc: exit during play.
Menus: W/S or Up/Down to select, Left/Right to adjust, Enter/Space to activate,
Esc/Backspace to go back. Mouse and controller input are also supported.

Collect healthy food, avoid obstacles, and keep running. Graphics options include
quality presets, render scale, VSync, shadows, reflections, ambient occlusion,
material detail, foliage particles and an optional PSX-inspired retro mode.
Settings are stored in game.cfg beside the executable. Delete that file to reset
preferences. Keep the game folder writable if you want settings to persist.

License and source
------------------
Food Run's original code is free software under GNU GPL version 3 or, at your
option, any later version. See LICENSE. Distributed without warranty.
Third-party models and libraries retain their own licenses; see
THIRD_PARTY_NOTICES.md, licenses/, and assets/models/*/license.txt. The road barrier
model is licensed for noncommercial use only.

Project and matching source releases: https://github.com/pedrolp22/foodrun
Each release provides foodrun-<version>-source.zip alongside the binary ZIPs.
If the repository has not been renamed yet, use https://github.com/pedrolp22/subway.
