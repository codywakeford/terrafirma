#!/usr/bin/env bash
# The Lab, headless: for every scenario in the game's lab.json files, build
# its own world, let tf_lab build it and wait out its `expect`, and report.
# Scenarios without an expect are listed as visual. Needs bin/luanti.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

ids=$(python3 -I - "$root/games/terrafirma/mods" <<'PY'
import json, sys, pathlib
for f in sorted(pathlib.Path(sys.argv[1]).glob("*/lab.json")):
    for s in json.load(open(f))["scenarios"]:
        print(s["id"])
PY
)

passed=0; failed=0; visual=0
for id in $ids; do
	world="$work/$id"
	mkdir -p "$world"
	printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\ncreative_mode = true\n' > "$world/world.mt"
	printf 'mg_name = singlenode\ntf_lab = %s\ntf_lab_check = true\n[end_of_params]\n' "$id" > "$world/map_meta.txt"
	port=$((30700 + RANDOM % 500))
	# no settings but the world's: a check can't depend on the minetest.conf of whoever runs it
	timeout 60 "$root/bin/luanti" --server --config /dev/null --world "$world" --port "$port" > "$world/log.txt" 2>&1 || true
	line="$(grep -oE "LAB $id .*" "$world/log.txt" || true)"
	case "$line" in
		*" ok") passed=$((passed + 1)) ;;
		*" visual") visual=$((visual + 1)); echo "  $id: visual" ;;
		*" fail "*) failed=$((failed + 1)); echo "  ${line#LAB }" ;;
		*) failed=$((failed + 1)); echo "  $id: no result"; grep -E "ERROR" "$world/log.txt" | head -5 | sed 's/^/    /' ;;
	esac
done
echo "lab check: $passed passed, $failed failed, $visual visual only"
[ "$failed" -eq 0 ]
