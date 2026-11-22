#!/usr/bin/env python3
"""Archive the exact release tag's tracked source, with a SHA-256 checksum."""

import argparse
import hashlib
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+", args.tag):
        parser.error("Expected a version tag such as v0.1.0")
    root = Path(__file__).resolve().parent.parent
    cmake = subprocess.check_output(["git", "show", f"{args.tag}:CMakeLists.txt"], cwd=root, text=True)
    version = re.search(r"project\(foodrun VERSION (\d+\.\d+\.\d+)", cmake).group(1)
    if args.tag != f"v{version}":
        parser.error(f"Tag {args.tag} does not match project version {version}")
    args.directory.mkdir(parents=True, exist_ok=True)
    archive = args.directory.resolve() / f"foodrun-{version}-source.zip"
    subprocess.run(["git", "archive", "--format=zip", f"--prefix=foodrun-{version}-source/",
                    f"--output={archive}", args.tag], cwd=root, check=True)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
    print(f"Created {archive.name} from {args.tag}")


if __name__ == "__main__":
    main()
