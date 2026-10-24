#!/usr/bin/env bash
# Flight and space on a live terra world: an autopilot flies up over the
# blocks to the boundary, into a bubble of space (tf_flight's space.lua), to a
# clear orbit, merges and splits bubbles, and comes back down to the blocks.
# Builds first.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/build" -j > /dev/null
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
world="$work/world"
mkdir -p "$world/worldmods"
cp -r "$root/scripts/flight-check/probe" "$world/worldmods/probe"
printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
printf 'mg_name = terra\nmgterra_code = TF5.dGVycmE\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"
port=$((31200 + RANDOM % 500))
timeout 300 "$root/bin/luanti" --server --world "$world" --port "$port" 2>&1 | tee "$work/server.txt" | grep -oE 'FLIGHT .*' > "$work/log.txt" || true
cat "$work/log.txt"
python3 -I - "$work" <<'PY'
import sys, re
work = sys.argv[1]
log = open(f"{work}/log.txt").read()
bad = []
def find(pat): return re.search(pat, log)
m = find(r"FLIGHT map (-?[\d.]+) (-?[\d.]+)")
if not m or (float(m.group(1)), float(m.group(2))) != (1234.25, -567.5): bad.append(f"the map ↔ planet round trip moved: {m and m.group(0)}")
u = find(r"FLIGHT boundary (\d+) (\d+)")
if not u: bad.append("never reached the boundary")
elif int(u.group(2)) > 341: bad.append(f"crossed the map at {u.group(2)} blocks/s (held to 340)")
if not find(r"FLIGHT orbit (\d+) (\d+) (\d+)"): bad.append("never reached orbit")
c = find(r"FLIGHT coasted (-?\d+) (\w+)")
if not c or int(c.group(1)) < 130000: bad.append(f"the orbit didn't hold: {c and c.group(0)}")
elif c.group(2) != "kept": bad.append("the bubble lost what was in it while coasting")
if not find(r"FLIGHT buildable true"): bad.append("a clear orbit should be buildable")
if not find(r"FLIGHT merged \d+"): bad.append(f"two bubbles closing never merged: {find(r'FLIGHT merged.*') and find(r'FLIGHT merged.*').group(0)}")
if not find(r"FLIGHT split own"): bad.append("something drifting out of a shared bubble didn't get its own")
d = find(r"FLIGHT down (-?\d+) (-?\d+) (\d+)")
if not d: bad.append("never came back to the blocks")
elif abs(int(d.group(2)) - 30000) > 2000: bad.append(f"came down at {d.group(2)} m, not at the boundary")
if bad: print(open(f"{work}/server.txt").read()[-3000:])
print("flight check: " + ("ok" if not bad else "FAILED"))
for b in bad: print("  " + b)
sys.exit(1 if bad else 0)
PY
