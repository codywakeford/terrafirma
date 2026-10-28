#!/usr/bin/env bash
# Measure the cube-sphere's cell distortion before changing its mapping.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/.output/planet-grid"
mkdir -p "$out"
g++ -std=c++17 -O2 -I"$root/apps/game/src" \
	"$root/apps/game/src/terragen/sphere.cpp" \
	"$root/scripts/planet-grid-check/grid.cpp" \
	-o "$out/audit"
"$out/audit" "$out"
column -s, -t "$out/summary.csv"
