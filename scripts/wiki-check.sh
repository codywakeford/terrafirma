#!/usr/bin/env bash
# The wiki's demos, end to end: every spec the page would make (reactions
# and Lab demos, from ui/src/wiki/demos.ts) run in the engine's sandbox for
# a simulated minute. Fails if any can't be made or stepped.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
g++ -std=c++17 -O2 -I"$root/src" -I"$root/lib/jsoncpp" "$root"/src/matter/*.cpp "$root"/src/sandbox/*.cpp \
	"$root/lib/jsoncpp/jsoncpp.cpp" "$root/scripts/wiki-check/run.cpp" -o "$work/run"
bun "$root/scripts/wiki-check/specs.ts" > "$work/specs.json"
"$work/run" "$root/matter" < "$work/specs.json"
