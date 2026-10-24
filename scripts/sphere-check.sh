#!/usr/bin/env bash
# The sphere spike (planning/sprint-25-sphere.md), end to end:
# - the geometry (terragen::Sphere: faces, edges, corners, the lens in
#   doubles and floats): its unit tests;
# - the shaders' copy of the lens (client/shaders/sphere/lens.glsl) compiles;
# - drawn: looking straight down from 500 m, off-screen (xvfb), a tiny
#   planet's face is a disc with sky round it, where the flat world is
#   ground to the corners; and from 2.5 km over a 2.5 km planet the far view
#   draws the whole face as a ball, where the near world alone is a speck.
# Builds the engine first. Runs from a scratch copy of the tree, so a game
# already running (its browser cache) doesn't get in the way.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/build" -j > /dev/null
"$root/bin/luanti" --run-unittests --test-module TestSphere 2>&1 | grep -E "Module TestSphere|FAIL" \
	| tee /dev/stderr | grep -q "TestSphere passed" || { echo "sphere check: unit tests FAILED"; exit 1; }

work="$(mktemp -d)"
trap '[ -n "${KEEP:-}" ] && echo "kept $work" || rm -rf "$work"' EXIT
{ printf '#version 330\nuniform mat4 mWorldViewProj;\n'; cat "$root/client/shaders/sphere/lens.glsl"; printf 'void main() {}\n'; } > "$work/lens.vert"
glslangValidator "$work/lens.vert" > /dev/null || { echo "sphere check: lens.glsl doesn't compile"; exit 1; }
echo "sphere check: lens.glsl compiles"

tree="$work/tree"
mkdir -p "$tree/bin"
for f in "$root"/*; do
	case "$(basename "$f")" in bin|cache|worlds|minetest.conf|debug.txt) ;; *) ln -s "$f" "$tree/" ;; esac
done
for f in "$root"/bin/*; do ln -s "$f" "$tree/bin/"; done # the browser's files beside the binary
rm "$tree/bin/luanti" && cp "$root/bin/luanti" "$tree/bin/"

# shot <name> <terra_sphere_spike> <mapgen> <far_view> <y>: a fresh world each, so none starts where another left off
shot() {
	local world="$work/$1.world"
	mkdir -p "$world/worldmods"
	cp -r "$root/scripts/sphere-check/probe" "$world/worldmods/sphere_probe"
	printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\ncreative_mode = true\nenable_damage = false\n' > "$world/world.mt"
	if [ "$3" = terra ]; then
		printf 'mg_name = terra\nmgterra_code = TF5.dGVycmE\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"
	else
		printf 'mg_name = flat\nseed = 1\n[end_of_params]\n' > "$world/map_meta.txt"
	fi
	printf 'screen_w = 1200\nscreen_h = 700\nviewing_range = 1000\nfov = 72\nfar_view = %s\nenable_clouds = false\nmute_sound = true\nclient_mapblock_limit = 20000\nmax_block_send_distance = 64\nmax_block_generate_distance = 64\nfree_move = true\nterra_sphere_spike = %s\nsphere_probe_y = %s\n' "$4" "$2" "$5" > "$work/$1.conf"
	(cd "$tree" && env -u WAYLAND_DISPLAY -u XDG_SESSION_TYPE timeout 140 xvfb-run -a -s "-screen 0 1200x700x24" bash -c \
		"./bin/luanti --config '$work/$1.conf' --go --world '$world' --name probe > '$work/$1.log' 2>&1 & sleep 75; import -window root '$work/$1.png'; kill %1")
	[ -s "$work/$1.png" ] || { echo "sphere check: no picture ($1); log:"; tail -20 "$work/$1.log"; exit 1; }
}
shot flat 0 flat false 500
shot round 500 flat false 500
# the far view on the sphere: from 2.5 km over a 2.5 km planet, the whole face, not just the near world
shot near 2000 terra false 2500
shot far 2000 terra true 2500

python3 -I - "$work" <<'PY'
import subprocess, sys
work = sys.argv[1]
def pixel(name, x, y):
    out = subprocess.run(["convert", f"{work}/{name}.png", "-format", f"%[fx:int(255*p{{{x},{y}}}.r)] %[fx:int(255*p{{{x},{y}}}.g)] %[fx:int(255*p{{{x},{y}}}.b)]", "info:"],
                         capture_output=True, text=True, check=True).stdout.split()
    return tuple(int(v) for v in out)
def sky(rgb):  # the day sky: blue well above red
    r, g, b = rgb
    return b > 200 and b - r > 50
ok = True
for name, x, y, want in (
    ("flat", 600, 300, False),  # below the eye: ground
    ("flat", 150, 150, False),  # and far out toward the corners
    ("round", 600, 300, False), # below the eye: the planet
    ("round", 150, 150, True),  # its edge has fallen away: sky
    ("near", 600, 430, True),   # from 2.5 km the near world is a speck
    ("far", 600, 430, False),   # the far view draws the whole face round it
    ("far", 150, 150, True),    # and it's a ball, sky beyond
):
    rgb = pixel(name, x, y)
    good = sky(rgb) == want
    print(f"sphere check: {name} ({x},{y}) {rgb} {'sky' if want else 'ground'} {'ok' if good else 'WRONG'}")
    ok &= good
sys.exit(0 if ok else "sphere check: FAILED")
PY
