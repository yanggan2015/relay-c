#!/usr/bin/env python3
"""Copy user-facing docs into the portable dist directory."""
from __future__ import annotations

import shutil
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: dist_copy_docs.py <distdir>", file=sys.stderr)
        return 2
    distdir = Path(sys.argv[1])
    root = Path(__file__).resolve().parent
    src = root / "dist_user_guide_zh.txt"
    if not src.is_file():
        print(f"missing {src}", file=sys.stderr)
        return 1
    distdir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, distdir / "使用说明.txt")
    shutil.copy2(src, distdir / "USAGE.zh-CN.txt")
    print(f"copied user guide -> {distdir / '使用说明.txt'}")
    print(f"copied user guide -> {distdir / 'USAGE.zh-CN.txt'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
