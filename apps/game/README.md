# Terra Firma game

This is the packaged game: `src/` is the Luanti fork and native systems,
`src/builtin/` is engine bootstrap Lua, `game/` is Terra Firma content, and
`ui/` is the Chromium interface.

Run `bun dev` or `bun run build` from the repository root. CMake builds the
native executable into `.output/bin`; `scripts/stage-runtime.sh` assembles
engine Lua, the game, and shared assets into `.output/` for run-in-place
launches. Production UI builds into `.output/ui/dist`.

`registry/` holds separately installable content. `library/` holds vendored
dependencies. Saved worlds and account data are local runtime state.
