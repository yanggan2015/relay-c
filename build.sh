#!/usr/bin/env bash
# Build portable binaries only (no source in zip).
#   ./build.sh           # build + windows zip
#   ./build.sh release   # build + upload zip to PUBLIC releases repo
#
# Source repo is private. Releases go to a separate public repo so anyone
# can download executables without seeing source. See DESIGN.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

MODE="${1:-build}"
if [ "$MODE" = "release" ] || [ "$MODE" = "build" ] || [ "$MODE" = "help" ] || [ "$MODE" = "-h" ] || [ "$MODE" = "--help" ]; then
  :
else
  echo "Usage: ./build.sh [build|release]"
  exit 1
fi
if [ "$MODE" = "help" ] || [ "$MODE" = "-h" ] || [ "$MODE" = "--help" ]; then
  echo "Usage: ./build.sh [build|release]"
  echo "  build    Compile, test, package windows zip under output/ (no source)"
  echo "  release  Same as build, upload binary zip to PUBLIC releases repo"
  echo
  echo "Env:"
  echo "  PLATFORM=windows|linux          default: windows"
  echo "  GITHUB_RELEASES_REPO=owner/name public repo for binary downloads"
  echo "  SKIP_UPLOAD=1                   package only"
  echo "  SKIP_TAG=1                      skip git tag on source repo"
  echo "  SKIP_TESTS=1                    skip tests"
  exit 0
fi

OUTDIR="${OUTDIR:-output}"
BUILDDIR="$OUTDIR/build"
DISTDIR="$OUTDIR/relay-c"
MINGW="${MINGW:-/c/msys64/mingw64}"
MSYS_USR="${MSYS_USR:-/c/msys64/usr}"
PLATFORM="${PLATFORM:-windows}"
GITHUB_SOURCE_REPO="${GITHUB_SOURCE_REPO:-yanggan2015/relay-c}"
GITHUB_RELEASES_REPO="${GITHUB_RELEASES_REPO:-yanggan2015/relay-c-releases}"
SKIP_UPLOAD="${SKIP_UPLOAD:-0}"
SKIP_TAG="${SKIP_TAG:-0}"
SKIP_TESTS="${SKIP_TESTS:-0}"

export PATH="$MINGW/bin:$MSYS_USR/bin:$PATH"

# MSYS non-login shells may get a broken HOME (e.g. C:Usersname). Fix so
# `git tag -a` can read the Windows user .gitconfig.
if [ -n "${USERPROFILE:-}" ] && { [ -z "${HOME:-}" ] || [[ "$HOME" != *"/"* && "$HOME" != *"\\"* ]]; }; then
  export HOME="$USERPROFILE"
fi
if [ -z "${HOME:-}" ] && [ -d /c/Users ]; then
  # last resort: common Windows profile path under MSYS
  for d in /c/Users/*; do
    if [ -f "$d/.gitconfig" ]; then export HOME="$d"; break; fi
  done
fi

echo "========================================"
echo " relay-c build.sh ($MODE)"
echo " PLATFORM=$PLATFORM"
echo " source repo  (private): $GITHUB_SOURCE_REPO"
echo " release repo (public) : $GITHUB_RELEASES_REPO"
echo " OUTDIR=$OUTDIR"
echo "========================================"
echo

if [ "$PLATFORM" != "windows" ]; then
  echo "[ERROR] PLATFORM=$PLATFORM is not implemented yet."
  echo "Only windows packages are produced today. See DESIGN.md."
  exit 1
fi

if ! command -v gcc >/dev/null 2>&1; then
  echo "[ERROR] gcc not found. Install MSYS2 mingw64 and run install_deps.bat"
  exit 1
fi
if ! command -v mingw32-make >/dev/null 2>&1 && ! command -v make >/dev/null 2>&1; then
  echo "[ERROR] make / mingw32-make not found"
  exit 1
fi
MAKE=mingw32-make
command -v mingw32-make >/dev/null 2>&1 || MAKE=make

if ! pkg-config --exists libcjson; then
  echo "[ERROR] libcjson not found (pkg-config). Run install_deps.bat"
  exit 1
fi

PY="$ROOT/tools/run_python.sh"
chmod +x "$PY" "$ROOT/build.sh" "$ROOT/tools/run_python.sh" 2>/dev/null || true
if ! "$PY" -c "print(1)" >/dev/null 2>&1; then
  echo "[ERROR] python not found (needed for version embed)."
  exit 1
fi
echo "Using Python via: $PY"

# Avoid Permission denied when exe is still running
taskkill //IM relay-c.exe //F >/dev/null 2>&1 || true

echo "[1/5] clean output/ ..."
$MAKE OUTDIR="$OUTDIR" MINGW="$MINGW" clean </dev/null

echo
echo "[2/5] generate version + compile -> $BUILDDIR ..."
"$PY" tools/gen_version.py -o include/version_gen.h
rm -f "$BUILDDIR/src/main.o"
$MAKE OUTDIR="$OUTDIR" MINGW="$MINGW" all </dev/null

VERSION="$(tr -d '\r' < "$OUTDIR/VERSION.txt" | head -n 1)"
if [ -z "$VERSION" ]; then
  echo "[ERROR] VERSION.txt missing after gen_version.py"
  exit 1
fi
echo "Build version: $VERSION"

if [ "$SKIP_TESTS" != "1" ]; then
  echo
  echo "[3/5] unit tests ..."
  # tests read/write boards.json (gitignored); seed from example when missing
  if [ ! -f boards.json ]; then
    cp -f boards.json.example boards.json
  fi
  $MAKE OUTDIR="$OUTDIR" MINGW="$MINGW" test </dev/null
else
  echo
  echo "[3/5] tests skipped (SKIP_TESTS=1)"
fi

echo
echo "[4/5] zip portable BINARY package (no source) ..."
if [ ! -d "$DISTDIR" ]; then
  echo "[ERROR] missing $DISTDIR (package step failed)"
  exit 1
fi

if [ -d "$DISTDIR/src" ] || [ -d "$DISTDIR/include" ] || [ -f "$DISTDIR/Makefile" ]; then
  echo "[ERROR] dist dir looks like it contains source; abort packaging"
  exit 1
fi
if [ ! -f "$DISTDIR/USAGE.zh-CN.txt" ]; then
  echo "[ERROR] missing $DISTDIR/USAGE.zh-CN.txt — user guide is required in release package"
  exit 1
fi

ZIP_NAME="relay-c-${PLATFORM}-${VERSION}.zip"
ZIP_PATH="$OUTDIR/$ZIP_NAME"
rm -f "$ZIP_PATH" "$OUTDIR/relay-c.zip" "$OUTDIR/relay-c-${PLATFORM}.zip"
export OUTDIR ZIP_NAME PLATFORM
"$PY" - <<'PY'
import os, zipfile, shutil
outdir = os.environ["OUTDIR"]
zip_name = os.environ["ZIP_NAME"]
platform = os.environ["PLATFORM"]
folder = os.path.join(outdir, "relay-c")
blocked = {"src", "include", "tests", "third_party", ".git"}
zip_path = os.path.join(outdir, zip_name)
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(folder):
        dirs[:] = [d for d in dirs if d not in blocked]
        for name in files:
            if name.endswith((".c", ".h", ".o", ".a")):
                raise SystemExit(f"refusing to package source-like file: {name}")
            full = os.path.join(root, name)
            arc = os.path.relpath(full, outdir).replace("\\", "/")
            zf.write(full, arc)
alias = os.path.join(outdir, f"relay-c-{platform}.zip")
shutil.copy2(zip_path, alias)
shutil.copy2(zip_path, os.path.join(outdir, "relay-c.zip"))
print("wrote", zip_path)
PY

if [ ! -f "$ZIP_PATH" ]; then
  echo "[ERROR] zip not created: $ZIP_PATH"
  exit 1
fi

echo
echo "Embedded version check:"
"$DISTDIR/relay-c.exe" version || true

if [ "$MODE" = "release" ]; then
  echo
  echo "[5/5] upload binary to PUBLIC releases repo ($GITHUB_RELEASES_REPO) ..."
  echo "      (source repo $GITHUB_SOURCE_REPO stays private; no source uploaded)"
  if [ "$SKIP_UPLOAD" = "1" ]; then
    echo "SKIP_UPLOAD=1 - not uploading"
  else
    if ! command -v gh >/dev/null 2>&1; then
      if [ -x "/c/Program Files/GitHub CLI/gh.exe" ]; then
        export PATH="/c/Program Files/GitHub CLI:$PATH"
      fi
    fi
    if ! command -v gh >/dev/null 2>&1; then
      echo "[ERROR] gh not found. Install GitHub CLI or set SKIP_UPLOAD=1"
      exit 1
    fi

    if ! gh repo view "$GITHUB_RELEASES_REPO" >/dev/null 2>&1; then
      echo "Creating public releases repo: $GITHUB_RELEASES_REPO"
      gh repo create "$GITHUB_RELEASES_REPO" --public \
        --description "Public binary releases for relay-c (source is private)" \
        --confirm
    fi

    TAG="v${VERSION}"

    if [ "$SKIP_TAG" = "1" ]; then
      echo "SKIP_TAG=1 - not tagging source repo"
    else
      echo "Tagging private source repo: $TAG"
      if git rev-parse "$TAG" >/dev/null 2>&1; then
        echo "  local tag $TAG already exists"
      else
        git tag -a "$TAG" -m "release ${VERSION} (${PLATFORM})"
        echo "  created annotated tag $TAG at $(git rev-parse --short HEAD)"
      fi
      if [ -n "$(git status --porcelain 2>/dev/null || true)" ]; then
        echo "  [WARN] working tree not clean; tag points to current HEAD anyway"
      fi
      # GitHub HTTPS from some networks resets; retry a few times, then continue
      # so the public binary upload still happens.
      PUSH_OK=0
      for attempt in 1 2 3; do
        echo "  git push tag (attempt $attempt/3) ..."
        if git push origin "refs/tags/$TAG"; then
          PUSH_OK=1
          break
        fi
        sleep $((attempt * 2))
      done
      if [ "$PUSH_OK" = "1" ]; then
        echo "  pushed tag -> https://github.com/${GITHUB_SOURCE_REPO}/tree/${TAG}"
      else
        echo "  [WARN] failed to push tag to source repo (network). Local tag kept."
        echo "  Retry later: git push origin refs/tags/$TAG"
      fi
    fi

    NOTES_FILE="$OUTDIR/RELEASE_NOTES_${VERSION}.md"
    cat > "$NOTES_FILE" <<EOF
## relay-c ${PLATFORM} \`${VERSION}\`

**Binary-only release** (source code is private / closed-source).

| Item | Value |
|------|-------|
| Platform | **${PLATFORM}** |
| Tag | \`${TAG}\` |
| Build time (UTC) | see \`VERSION.txt\` in zip |
| Valid for | **90 days** from compile time |
| Contact | yanggan2015@foxmail.com |

### Download
- Asset: \`${ZIP_NAME}\` (executable + runtime DLLs + examples + \`使用说明.txt\`)
- **No source code** is included.
- Open \`使用说明.txt\` inside the zip for the full user guide.

### Install (Windows)
1. Unzip \`${ZIP_NAME}\`
2. Copy \`boards.json.example\` -> \`boards.json\` and edit serial ports
3. Run \`run.bat\` or \`relay-c.exe\`

Linux builds will use asset name \`relay-c-linux-<VERSION>.zip\` when available.

All releases: https://github.com/${GITHUB_RELEASES_REPO}/releases
EOF

    if gh release view "$TAG" --repo "$GITHUB_RELEASES_REPO" >/dev/null 2>&1; then
      echo "Release $TAG exists - uploading/replacing ${PLATFORM} asset"
      gh release upload "$TAG" "$ZIP_PATH" --repo "$GITHUB_RELEASES_REPO" --clobber
    else
      gh release create "$TAG" "$ZIP_PATH" \
        --repo "$GITHUB_RELEASES_REPO" \
        --title "relay-c ${VERSION} (${PLATFORM})" \
        --notes-file "$NOTES_FILE"
    fi
    echo "Public download: https://github.com/${GITHUB_RELEASES_REPO}/releases/tag/${TAG}"
  fi
else
  echo
  echo "[5/5] skip upload (use: ./build.sh release)"
fi

echo
echo "========================================"
echo " OK ($MODE)"
echo "  version       : $VERSION"
echo "  platform      : $PLATFORM"
echo "  zip (binary)  : $ZIP_PATH"
echo "  contains src? : NO"
if [ "$MODE" = "release" ] && [ "$SKIP_UPLOAD" != "1" ]; then
  echo "  tag           : v${VERSION}"
  echo "  public URL    : https://github.com/${GITHUB_RELEASES_REPO}/releases/tag/v${VERSION}"
fi
echo "========================================"
