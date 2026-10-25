#!/usr/bin/env bash
# The planet's edges, end to end (planning/sprint-26-planet.md, phase 2): a
# small planet world on a headless server, and off every side of every face
# a walker crosses onto the next face, water flows over the edge, and sand
# lands past it (scripts/planet-check/probe). Builds the engine first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/planet-check/probe" "$world/worldmods/planet_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = flat\nseed = 1\nplanet_half = 64\nmg_flags = nocaves,nodungeons,light,nodecorations\n[end_of_params]\n' > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 \
	| tee "$work/server.txt" | grep -oE 'PLANET .*' | tee "$work/log.txt" || true

line="$(grep -E '^PLANET pass' "$work/log.txt" || true)"
if [ -z "$line" ]; then
	echo "planet check: no result; the server said:"
	grep -E "ERROR|error" "$work/server.txt" | head -20
	exit 1
fi
fail="$(echo "$line" | awk '{print $5}')"
[ "$fail" = 0 ] && echo "planet check: ok" || { echo "planet check: FAILED"; exit 1; }
