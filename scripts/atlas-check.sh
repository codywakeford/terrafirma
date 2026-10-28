#!/usr/bin/env bash
# The atlas's mapping, locked (planning/sprint-30-atlas.md, phase 1): the
# engine's TestAtlas, then the lab's survey of every supported planet size
# in both layouts against scripts/atlas-check/golden.json. A difference is a
# change to every world's planet: if it's meant, `--update` and say why.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/.output/atlas-lab"
mkdir -p "$out"

cmake --build "$root/.output/build" -j > /dev/null
"$root/.output/bin/luanti" --test-module TestAtlas

g++ -std=c++17 -O2 -Wall -Wextra -I"$root/apps/game/src" \
	"$root/apps/game/src/terragen/atlas.cpp" "$root"/scripts/atlas-lab/*.cpp -o "$out/atlas"
for layout in 0 1; do
	"$out/atlas" sweep 0 0.01 0.01 85 64 64 0 "$layout" > "$out/sweep-$layout.json"
done
bun "$root/scripts/atlas-check/compare.ts" "$out/sweep-0.json" "$out/sweep-1.json" "$@"
