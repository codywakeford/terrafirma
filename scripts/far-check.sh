#!/usr/bin/env bash
# The far view against the world: generate areas of a terra world headless
# and check, column by column, that core.terra.surface (what the far view
# draws) is what the generator made: the ground's block and height, the
# water over it, and the top of each tree. Builds the engine first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/far-check/probe" "$world/worldmods/far_probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = terra\nmgterra_code = TF5.dGVycmE\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"

port=$((30700 + RANDOM % 500))
timeout 300 "$root/bin/luanti" --server --world "$world" --port "$port" 2>&1 | grep -oE '(FAR|ERROR).*' | tee "$work/log.txt"

python3 -I - "$work/log.txt" <<'PY'
import sys
line = next((l for l in open(sys.argv[1]) if l.startswith("FAR columns")), None)
if not line:
    sys.exit("far check: no result (did the server start?)")
f = line.split()
n = dict(zip(f[1::2], f[2::2]))
columns = int(n["columns"])
ok = True
# Exact but for one thing no generator can know: the order the server
# generated chunks in. A tree at a chunk's edge grows into the next chunk if
# that isn't generated yet (and keeps its blocks there), and where crowns
# from two chunks overlap the first planted wins. Within a chunk the far view
# plants in the game's order, so it's exact there.
for what, need in (("ground", 0.9999), ("water", 0.9999), ("tree", 0.99), ("height", 0.99)):
    share = int(n[what]) / columns
    print(f"far check: {what} {int(n[what])} of {columns} columns, {share:.3%} (need {need:.2%})")
    ok &= share >= need
sys.exit(0 if ok else "far check: FAILED")
PY
