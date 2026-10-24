#!/usr/bin/env bash
# What must hold before a release is tagged, checked here in minutes rather
# than found by CI in half an hour: the engine builds, its tests pass, the UI
# typechecks and builds, and a world boots headless without Lua errors.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"

cmake --build build -j"$(nproc)"
./bin/luanti --test-module TestOnline
./bin/luanti --test-module TestTerragen
(cd ui && bun run typecheck && bun run build)

world=$(mktemp -d)
trap 'rm -rf "$world"' EXIT
set +e
timeout 20 ./bin/luanti --server --gameid terrafirma --world "$world/w" --port 30501 >"$world/boot.log" 2>&1
set -e
grep -q 'listening on' "$world/boot.log" || { cat "$world/boot.log"; echo "gate: the server didn't start" >&2; exit 1; }
if grep -q 'ERROR\[' "$world/boot.log"; then grep 'ERROR\[' "$world/boot.log"; echo "gate: errors booting a world" >&2; exit 1; fi
echo "gate: ok"
