#!/usr/bin/env bash
# Flight on a planet (planning/sprint-27-spheres-stable.md): on a live terra
# planet world, the planet's own physics takes a craft up from the ground,
# round once in orbit over the four faces of the equator, and back down
# (scripts/flight-check/probe). Builds first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
luajit "$root/scripts/flight-check/trajectory.lua" "$root/apps/game/game/mods/tf_flight"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/flight-check/probe" "$world/worldmods/probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = terra\nmgterra_code = TF5.dGVycmE\nseed = 1\nplanet_half = 2048\nplanet_mapping = cobe-v1\n[end_of_params]\n' > "$world/map_meta.txt"
port=$((31200 + RANDOM % 500))
# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
timeout 300 "$root/.output/bin/luanti" --server --config /dev/null --world "$world" --port "$port" 2>&1 | tee "$work/server.txt" | grep -oE 'FLIGHT .*' > "$work/log.txt" || true
cat "$work/log.txt"
if [ ! -s "$work/log.txt" ]; then
	echo "flight check: server produced no probe output; startup errors:"
	grep -E 'ERROR|Error|error|WARNING' "$work/server.txt" | head -30 || true
fi
python3 -I - "$work" <<'PY'
import sys, re
work = sys.argv[1]
log = open(f"{work}/log.txt").read()
bad = []
def find(pat): return re.search(pat, log)
p = find(r"FLIGHT planet (\d+) ([\d.]+) ([\d.]+)")
if not p: bad.append("no planet")
else:
    if abs(int(p.group(1)) - 2607) > 2: bad.append(f"core.orbit's planet is {p.group(1)} m, not the world's 2607")
    if abs(float(p.group(2)) - 1.225) > 0.01: bad.append(f"the air at the ground is {p.group(2)}")
    if float(p.group(3)) > 0.01: bad.append(f"the air at 2.5 km is still {p.group(3)}: orbit would drag down")
c = find(r"FLIGHT climbed (\d+) (\d+)")
if not c or abs(int(c.group(1)) - 2500) > 100: bad.append(f"didn't climb to 2.5 km: {c and c.group(0)}")
o = find(r"FLIGHT orbit ([\d,]+) (-?\d+) (-?\d+)")
if not o: bad.append("never orbited")
else:
    faces = [int(f) for f in o.group(1).split(",")]
    # the equator's four faces in turn, east (from wherever the climb left it)
    if len(faces) < 4 or any(f > 3 for f in faces[:4]) or any((faces[i] + 1) % 4 != faces[i + 1] for i in range(3)):
        bad.append(f"the orbit passed over faces {o.group(1)}, not the equator's four in turn")
    if int(o.group(2)) < 2200 or int(o.group(3)) > 2800: bad.append(f"the orbit didn't hold its height: {o.group(2)}–{o.group(3)} m")
l = find(r"FLIGHT landed (-?\d+)")
if not l or abs(int(l.group(1))) > 2: bad.append(f"didn't come back down to the ground: {l and l.group(0)}")
for b in bad: print("flight check:", b)
print("flight check:", "FAILED" if bad else "ok")
sys.exit(1 if bad else 0)
PY
