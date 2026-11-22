#!/usr/bin/env python3
"""Check runtime ZIPs and optionally run their extracted executables."""

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import zipfile


def verify(archive, smoke_test=False, version_check=False):
    match = re.fullmatch(r"foodrun-(\d+\.\d+\.\d+)-(linux|windows)-([\w]+)\.zip", archive.name)
    if not match:
        raise ValueError(f"Unexpected package name: {archive.name}")
    version, platform, _ = match.groups()
    checksum = archive.with_suffix(".zip.sha256")
    expected = checksum.read_text().split()[0]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise ValueError(f"Checksum mismatch: {archive}")
    with zipfile.ZipFile(archive) as package:
        paths = [PurePosixPath(item.filename) for item in package.infolist()]
        if any(path.is_absolute() or ".." in path.parts for path in paths):
            raise ValueError("Unsafe path in ZIP")
        roots = {path.parts[0] for path in paths}
        if roots != {archive.stem}:
            raise ValueError(f"Expected a single {archive.stem} folder, found {roots}")
        files = {str(path.relative_to(archive.stem)) for path in paths}
        executable = "foodrun.exe" if platform == "windows" else "foodrun"
        required = {
            executable, "README.txt", "LICENSE", "THIRD_PARTY_NOTICES.md",
            "licenses/SDL2.txt", "licenses/glad.txt", "licenses/stb_image.txt",
            "assets/models/gato/gato.gltf", "assets/models/gato/gato.bin", "assets/models/gato/gato.png",
            "assets/textures/collectibles/water.png", "assets/materials/road_surface.png",
        }
        for model in ("barrier", "bench", "street_lamp", "traffic_cone", "trash_can", "tree"):
            required.add(f"assets/models/{model}/license.txt")
        if required - files:
            raise ValueError(f"Missing runtime files: {sorted(required - files)}")
        for path in paths:
            if any(part in {".git", ".idea", ".cooked", "src", "include", "shaders", "tests", "_deps"}
                   for part in path.parts) or path.name in {"game.cfg", "game.cfg.tmp", "CMakeCache.txt", "CMakeLists.txt"}:
                raise ValueError(f"Development/personal file in ZIP: {path}")
        if smoke_test or version_check:
            host_platform = "windows" if os.name == "nt" else "linux"
            if host_platform != platform:
                print(f"Verified {archive.name} (runtime check skipped on {host_platform})")
                return
            # Spaces and a different working directory catch common packaging bugs.
            with tempfile.TemporaryDirectory(prefix="foodrun package ") as temporary:
                package.extractall(temporary)
                binary = Path(temporary) / archive.stem / executable
                binary.chmod(0o755)
                result = subprocess.run([str(binary), "--version"], cwd=temporary, check=True,
                                        capture_output=True, text=True, timeout=30)
                if result.stdout.strip() != f"Food Run {version}":
                    raise ValueError(f"Unexpected version: {result.stdout}")
                if smoke_test:
                    subprocess.run([str(binary), "--smoke-test"], cwd=temporary, check=True, timeout=60)
    print(f"Verified {archive.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--smoke-test", action="store_true")
    parser.add_argument("--version-check", action="store_true")
    args = parser.parse_args()
    archives = sorted(path for path in args.directory.glob("foodrun-*.zip") if "-source.zip" not in path.name)
    if not archives:
        parser.error("No Food Run runtime ZIPs found")
    for archive in archives:
        verify(archive, args.smoke_test, args.version_check)


if __name__ == "__main__":
    main()
