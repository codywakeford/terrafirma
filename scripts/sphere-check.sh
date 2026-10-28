#!/usr/bin/env bash
# The planet (planning/sprint-26-planet.md), drawn, end to end:
# - the geometry (terragen::Sphere, planet::Planet: faces, the net, steps
#   across edges, corners, the lens in doubles and floats): its unit tests;
# - the shaders' lens (planet::Planet::glslAxes + client/shaders/sphere/
#   lens.glsl, as every vertex shader gets them) compiles;
# - drawn, off-screen (xvfb), looking straight down: from 500 m a small
#   planet (planet_half 512) is a disc with sky round it, where a flat world
#   is ground to the corners; and 15 blocks from a corner of face 0, its
#   two neighbours (each face painted its own block) meet it round the
#   point; and a client walking itself off an edge where the grid turns is
#   carried onto the next face, looking on the way it was going. (The far
#   view on a planet needs terra on the sphere: phase 4.)
# Builds the engine first. Runs from a scratch copy of the tree, so a game
# already running (its browser cache) doesn't get in the way.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$root/.output/build" -j > /dev/null
for module in TestSphere TestPlanet; do
	"$root/.output/bin/luanti" --run-unittests --test-module $module 2>&1 | grep -E "Module $module|FAIL" \
		| tee /dev/stderr | grep -q "$module passed" || { echo "sphere check: $module FAILED"; exit 1; }
done

work="$(mktemp -d)"
trap '[ -n "${KEEP:-}" ] && echo "kept $work" || rm -rf "$work"' EXIT
# what every vertex shader gets: the axes (planet::Planet::glslAxes), then the lens
g++ -std=c++17 -I"$root/apps/game/src" "$root/scripts/sphere-check/axes.cpp" "$root/apps/game/src/planet/planet.cpp" \
	"$root/apps/game/src/terragen/sphere.cpp" -o "$work/axes"
{ printf '#version 330\nuniform mat4 mWorldViewProj;\n'; "$work/axes"; cat "$root/registry/shaders/sphere/lens.glsl"; printf 'void main() {}\n'; } > "$work/lens.vert"
glslangValidator "$work/lens.vert" > /dev/null || { echo "sphere check: lens.glsl doesn't compile"; exit 1; }
echo "sphere check: lens.glsl compiles"

# Agent-safe proof of the geometry and shared shader contract. The full check
# below launches a client and is reserved for the user-run visual gate.
if [ "${1:-}" = "--compile-only" ]; then
	exit 0
fi

tree="$work/tree"
mkdir -p "$tree/bin"
for d in builtin client fonts games locale matter textures misc ui; do
	ln -s "$root/.output/$d" "$tree/$d"
done
for f in "$root"/.output/bin/*; do ln -s "$f" "$tree/bin/"; done # the browser's files beside the binary
rm "$tree/bin/luanti" && cp "$root/.output/bin/luanti" "$tree/bin/"

# shot <name> <planet_half or 0> <y> [setting=value...]: a fresh world each, so none starts where another left off
shot() {
	local name="$1" half="$2" y="$3"
	shift 3
	local world="$work/$name.world"
	mkdir -p "$world/worldmods"
	cp -r "$root/scripts/sphere-check/probe" "$world/worldmods/sphere_probe"
	printf 'gameid = terrafirma\nbackend = sqlite3\nplayer_backend = sqlite3\nauth_backend = sqlite3\n' > "$world/world.mt"
	printf 'mg_name = flat\nseed = 1\nplanet_half = %s\n[end_of_params]\n' "$half" > "$world/map_meta.txt"
	{
		printf 'screen_w = 1200\nscreen_h = 700\nviewing_range = 1000\nfov = 72\nfar_view = false\nenable_clouds = false\n'
		printf 'mute_sound = true\nenable_damage = false\ncreative_mode = true\nfree_move = true\n'
		printf 'client_mapblock_limit = 20000\nmax_block_send_distance = 64\nmax_block_generate_distance = 64\n'
		printf 'sphere_probe_y = %s\n' "$y"
		for kv in "$@"; do printf '%s\n' "${kv/=/ = }"; done
	} > "$work/$name.conf"
	(cd "$tree" && env -u WAYLAND_DISPLAY -u XDG_SESSION_TYPE timeout 150 xvfb-run -a -s "-screen 0 1200x700x24" bash -c \
		"./bin/luanti --config '$work/$name.conf' --go --world '$world' --name probe > '$work/$name.log' 2>&1 & sleep 80; import -window root '$work/$name.png'; kill %1")
	[ -s "$work/$name.png" ] || { echo "sphere check: no picture ($name); log:"; tail -20 "$work/$name.log"; exit 1; }
}
shot flat 0 500
shot round 512 500
shot corner 512 150 sphere_probe_fx=0.985 sphere_probe_fz=0.985
# the client walking over an edge where the grid turns: carried onto face 4,
# looking the way it was going (west, in face 4's grid), not reset by the server
shot walk 512 0 sphere_probe_walk=true continuous_forward=true
walk="$(grep -oE 'SPHERE walk .*' "$work/walk.log" || true)"
echo "sphere check: $walk"
python3 -I - "$walk" <<'PY' || exit 1
import sys
f = sys.argv[1].split()
if len(f) < 9:
    sys.exit("sphere check: the walk didn't report")
face, lx, lz = f[3], float(f[5]), float(f[6])
ok = face == "4" and lx < -0.95 and abs(lz) < 0.2
print(f"sphere check: walked onto face {face}, looking ({lx}, {lz}): {'ok' if ok else 'WRONG (want face 4, looking west)'}")
sys.exit(0 if ok else 1)
PY

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
def snow(rgb):
    return min(rgb) > 170 and max(rgb) - min(rgb) < 70
def sand(rgb):
    r, g, b = rgb
    return r > 220 and g > 180 and b < 150
def clay(rgb):
    r, g, b = rgb
    return r > 170 and 90 < g < 160 and b < 100
def ground(rgb):  # anything but sky; face 0's snow under haze is pale blue
    return not sky(rgb) or snow(rgb)
ok = True
for name, x, y, what, test in (
    ("flat", 600, 300, "ground", ground),   # flat: ground below the eye
    ("flat", 150, 150, "ground", ground),   # and far out toward the corners
    ("round", 600, 300, "ground", ground),  # a small planet: below the eye
    ("round", 150, 150, "sky", sky),        # its edge has fallen away
    # 15 blocks from a corner of face 0: three faces round the point
    ("corner", 300, 500, "face 0 (snow)", snow),
    ("corner", 900, 450, "face 1 (sand)", sand),
    ("corner", 400, 150, "face 4 (clay)", clay),
):
    rgb = pixel(name, x, y)
    good = test(rgb)
    print(f"sphere check: {name} ({x},{y}) {rgb} {what} {'ok' if good else 'WRONG'}")
    ok &= good
sys.exit(0 if ok else "sphere check: FAILED")
PY
