#!/usr/bin/env bash
# Portable python launcher for make/build.sh under MSYS2.
set -e
if command -v python >/dev/null 2>&1; then
  exec python "$@"
fi
if command -v python3 >/dev/null 2>&1; then
  exec python3 "$@"
fi
if command -v py >/dev/null 2>&1; then
  exec py -3 "$@"
fi
if [ -x /c/Windows/py.exe ]; then
  exec /c/Windows/py.exe -3 "$@"
fi
echo "python not found" >&2
exit 1
