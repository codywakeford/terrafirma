#!/usr/bin/env bash
# The debug sphere, generated headless (planet_atlas_debug; sprint 30): the
# default world's atlas with no terrain, every cell painted by what it is;
# boxes where things meet, every node checked (scripts/atlas-debug-check/probe).
# Builds the engine first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/atlas-debug-check/probe" "$world/worldmods/atlas_debug_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
# TF5.dGVycmE: the default size, 8192 round: radius 8192 / 2π, the standard layout
atlas="atlas-v2 radius=1303.7972938088067 segments=24 capLatitude=85 core=325.94932345220167 rounding=0.95 height=256"
printf 'mg_name = terra\nmgterra_code = TF5.dGVycmE\nseed = 1\nchunksize = 1\nplanet_atlas_debug = true\nplanet_atlas = %s\n[end_of_params]\n' "$atlas" > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE 'ATLAS .*' | tee "$work/log.txt" || true

line="$(grep -E '^ATLAS pass' "$work/log.txt" || true)"
if [ -z "$line" ]; then
	echo "atlas debug check: no result; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
fail="$(echo "$line" | awk '{print $5}')"
[ "$fail" = 0 ] && echo "atlas debug check: ok" || { echo "atlas debug check: FAILED"; exit 1; }
