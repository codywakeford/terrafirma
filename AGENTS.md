# Terra Firma — Agent Brief

Terra Firma is an open-source survival voxel game: Minecraft-like, but built
around a world that is *simulated* rather than sprinkled with noise, and
survival that can be tuned anywhere from arcade to realistic. The aim is a
rich, complete base game, with a carefully chosen progression from the stone
age to the stars (see "The game"), built as built-ins that can each be
switched off. It is also a
world for AI agents to live in (ArcLabs' Axon labs plug in later).

It is a fork of the Luanti engine (C++, LGPL-2.1+). We keep Luanti's fast
engine — rendering, meshing, networking, map storage — and replace its shell:
all 2D UI is a Vue app drawn by an embedded Chromium, and world generation is
our own model.

Read this whole file before changing anything. It records decisions; if a
change contradicts one, raise it rather than drift.

---

## Repo map

```
src/              Luanti engine (upstream). Touch lightly; see "Engine policy".
src/shell/        OURS — the web UI host: Chromium (CEF), bridge, menus, screens, state stream
src/terragen/     OURS — the world model: a pure, fixed-point function of the world code (no engine deps)
scripts/assets/   texture import: Good Vibes at a pinned revision, plus derived rocks and ores
src/matter/       OURS — elements and substances, and the state they're in at any temperature and pressure
matter/           OURS — the building blocks as data: elements.json (all 118), substances.json (the modelled ones)
src/mapgen/mapgen_terra.*  OURS — the `terra` map generator: terrain/climate/biome from terragen
ui/               OURS — the Vue app (Vite + TypeScript): menus, HUD, in-game screens, gallery
games/terrafirma/ OURS — the game, in Lua: blocks, items, recipes, biomes, trees, survival systems
cmake/Modules/CEF.cmake   fetches the pinned CEF build
scripts/dev.ts    the dev loop behind `bun dev`
scripts/terragen-check.sh  generate a world headless and check it against standalone terragen
planning/         sprint plans and their outcomes
builtin/, games/devtest, irr/, lib/, android/   upstream; leave alone unless asked
```

## Running and checking

```bash
bun dev                 # rebuild engine, start Vite, launch the game against the dev server (hot reload in-game)
bun dev --go            # same, straight into a world
bun components          # the component gallery in a browser — every UI component, mock engine, no game
bun run build           # engine + production UI (ui/dist, served in-game as terra://ui/)

cmake --build build -j8                  # engine only
cd ui && bun run typecheck               # UI types (vue-tsc; TypeScript is pinned to 5.x — vue-tsc can't run on 7)
./bin/luanti --server --gameid terrafirma --world <tmp dir> --port <n>   # headless boot: catches Lua errors
./bin/luanti --test-module TestTerragen   # terragen unit tests (--run-unittests for the whole engine)
bun check:terragen      # world generation end to end: probed world == standalone terragen, + unit tests
bun check:lab           # every Lab scenario built headless; each `expect` checked
# tuning by eye: render a code's grid (relief, lakes, rivers) to a PNG and look at it
g++ -std=c++17 -O2 -Isrc src/terragen/*.cpp scripts/terragen-check/render.cpp -o /tmp/render && /tmp/render TF2.dGVycmE out.png
```

**How agents verify:** build the engine, typecheck and build the UI, boot the
game headless for Lua errors, run the terragen check after any generation
change, run the Lab check after any mechanic change, and use the gallery for
UI. **Do not launch the
game client or screenshot the user's desktop** — hand it over and the user
runs it and shares screenshots. Say plainly what was and wasn't run.

---

## Engine policy

- Our code lives in our folders. Upstream files get **small hooks** guarded by
  `#if USE_SHELL` (and later `USE_TERRAGEN`), so upstream merges stay cheap.
  Current hooks: `main.cpp` (CEF helper entry, `--shell-url`), `clientlauncher`
  (owns the Shell, swaps the main menu), `game.cpp` (Session lifetime, Esc /
  inventory keys, per-frame update), three draw sites (`game.cpp`,
  `guiEngine.cpp`, `renderingengine.cpp`), `inputhandler.cpp`, `hud.cpp` and
  `render/plain.cpp` (native HUD parts off), `defaultsettings.cpp`, CMake;
  `mapgen.cpp/.h` register the `terra` generator (enum + name + factory).
- Upstream C++ keeps upstream style. Our C++ (`src/shell`, `src/terragen`)
  follows "Code standards" below within C++ idiom: one concern per class,
  pimpl to keep CEF out of engine headers, RAII.
- Old UI code (main menu Lua, formspecs) is deleted only once its replacement
  is proven. The non-shell build is not a supported target going forward.
- Irrlicht here has **no `irr::` namespace** (`video::`, `gui::` are top level).
- CEF headers need C++20; our shell sources are compiled `-std=c++20`, set at
  the target in `src/CMakeLists.txt` (source properties are directory-scoped).

## The shell (src/shell)

One Chromium page lives for the whole session, rendered offscreen and drawn
over every frame. Each file owns one concern:

| File | Owns |
|---|---|
| `shell.*` | the composition root: CEF lifetime, page, `frame()`, `onEvent()`, `on/off/send` |
| `app.*` | process-level CEF hooks; the helper-process entry (`runHelper` first thing in `main`) |
| `client.*` | CEF's view of the browser: paint → Pixels, size, console/load errors to the log |
| `pixels.*` / `surface.*` | dirty-rect unpremultiply (LUT) → Irrlicht texture → drawn last |
| `input.*` | mouse to the page where it painted something opaque (or while a press is held); keyboard only when the page asks |
| `ime.*` | hidden IME element so SDL delivers typed text |
| `bridge.*` | intents in (JSON, registered verbs), messages out |
| `scheme.*` | `terra://ui/` (built UI) and `terra://item/` (icons) |
| `options.*` | the settings the page may read/change, with ranges |
| `menu.*` / `worlds.*` | the main menu loop and world list/create/delete; `worldgen.preview` (terragen fields for the create screen) |
| `session.*` | one running game: owns `state`, `items`, `screens`, `actions` and the in-game verbs |
| `state.*` | per-frame read of the local player; sends sections that changed **by value** |
| `items.*` | item label + icon (blocks rendered as cubes, read back, written as PNG) |
| `screens.*` | page screens backed by an invisible `GUIModalMenu` (real pause/cursor/Esc semantics); table of screens |
| `actions.*` | inventory move/craft/drop → the engine's own inventory actions |

Settings: `shell_url` (default `terra://ui/index.html`), `shell_gpu` (Chromium
GPU raster; set false if a driver misbehaves), `game_id` (default `terrafirma`).

### The bridge contract

`ui/src/bridge/types.ts` is the source of truth for every intent and message.
**Change both sides together.**

- **Intents** (page → engine) are answered by verbs registered with
  `shell.on(kind, fn)` by whoever owns that screen, and removed with `off` when
  it hands over. An intent nobody answers fails loudly in the page.
  Always on: `ready`, `capture`, `settings.get/set`, `open`.
  Menu: `worlds.*`, `play`, `lab.list`, `lab.enter`, `quit`, `worldgen.preview`. Session: `screen.open`,
  `screen.close`, `game.leave`, `game.quit`, `inventory.move/craft/drop`, `ui.send`.
- A verb throws `Refused("…")` for an expected no (shown to the player);
  anything else is logged as an engine error.
- **Slow verbs are async** (`shell.onAsync`): they run on the bridge's worker
  thread, one at a time, and answer on the main thread — the menu keeps
  drawing. They must be thread-safe (pure). `worldgen.preview` is one.
- **Messages** (engine → page): `mode`, `error`, `items`, `state` (sections:
  `hud`, `player`, `hotbar`, `inventory`), `screen`, `ui`. Messages sent before the
  page is ready are queued; `mode` is resent after every reload.
- **The UI channel: game Lua ↔ page.** `player:set_ui(key, value)` sends a
  JSON value to that player's page (`TOCLIENT_UI`; the page keeps the latest
  per key, null clears); the page's `ui.send { name, payload }` reaches
  `core.register_on_ui_event(function(player, name, payload))`
  (`TOSERVER_UI`). Keys and events are typed in `UiValues` / `UiEvents`. A
  screen can require a key (the Lab screen needs `lab`). This is how game
  systems feed the HUD.
- **The server is the authority.** The page never edits game state; it sends
  intents and renders what comes back. No optimistic updates.

## The UI (ui/)

```
ui/src/
  bridge/     contract types, Bridge(), useBridge(), keyboard capture (focus-driven)
  game/       store (all engine state, reactive), HUD, pause screen, inventory/
  menu/       main menu: home, worlds, create/ (steps Start · World · Rules), settings wrapper
  system/     the planetary system on the page: causes only (model.ts), cards from the engine (preview.ts), tree, card, history
  settings/   SettingsPanel — the one settings UI, used by menu and pause
  kit/        the component kit — import from "../kit"
  worldgen/   world config + codes (incl. star/tilt/year), sun.ts (the solar model),
              fields.ts (engine preview fields, overlays, painting), previewer (asks the
              engine), map/globe/star views
  gallery/    `bun components`: pages, mock engine
  theme.css   tokens: colours, fonts, the lighting model
```

- **Build every screen from the kit** (Button, Panel, Slider, Toggle, Field,
  Tabs, Segmented, Choice, IconTile, Section, Notice). New visual patterns become kit
  components, and get a gallery specimen.
- **One light, top-left.** Never hand-write a bevel: use `--raised` /
  `--sunk` (and `--ledge`); layer accents first:
  `box-shadow: inset 0 -4px 0 var(--accent), var(--raised)`.
- Hard edges (`--b: 2px`), no radius. Accent `rgb(0 148 210)` and the `»`
  chevron are the only brand marks. Fonts are bundled (Silkscreen display,
  JetBrains Mono body) — never load from the network.
- Labels never wrap in buttons; panels never scroll sideways; grid children
  get `min-width: 0`.
- The store holds `null` for anything the engine hasn't sent — render nothing
  rather than invent a value.
- Every component must render in the gallery against the mock engine
  (`gallery/mock.ts`). If it can't, its boundary is wrong. Keep the mock's
  intent table complete (it is typed against the contract). The mock answers
  `worldgen.preview` with a JavaScript stand-in (`gallery/mockWorld.ts`) —
  same shape, not terragen's numbers.
- Performance: the page is re-uploaded to the GPU when it repaints, so avoid
  continuous full-screen animation; prefer stepped motion; heavy compute goes
  in a Web Worker.

## The game (games/terrafirma)

Lua mods. `tf_world` has blocks, water, biomes, trees (`trees.lua`), placeholder
recipes and a placeholder starter kit, with Good Vibes textures (see
`assets/CREDITS.md`). `tf_sky` keeps each player's solar sky. `tf_lab` is the
Lab (below).

Biomes: `biomes.lua` registers one Lua biome per terragen class
(`terragen/world.cpp` `BIOME_NAMES`) — terragen decides where, Lua says what
it's made of; the `terra` generator refuses to start if one is missing, or if
the `mapgen_ice` alias (the polar wall) is absent.

**The Lab** (`tf_lab`, sprints 4–5): a page of scenarios, one mechanic each,
**each in its own tiny world**. The Lab page (`ui/src/lab`) lists every
scenario. Choosing one builds and enters its world (`lab.enter { id }` →
`<user>/lab`, `tf_lab = <id>` in the map settings), and the page stays put:
the game draws through the middle of it, with the scenario's controls
below. Choosing another leaves that world and enters the next. **A scenario
is data plus behaviour:**
- data, in the owning mod's `lab.json`: id, title, description, size,
  optional world and camera, and controls (`button`, `toggle`, `slider`,
  `choice`). The menu reads it before any game Lua runs (`src/shell/lab.*`).
- behaviour, in Lua: `tf_lab.load()` then
  `tf_lab.behaviour(id, { build, control?, enter?, expect? })`. Loading fails
  if the two disagree.

In the world, the camera orbits the scenario while the Lab view is open
(`set_ui "camera"`, run on the client by `shell/director.*`). Step in
closes the view, L reopens it. Restart and time speed
(`core.set_time_speed`) are always there. `bun check:lab` runs every
scenario in its own headless world and checks its `expect`. Systems that
need terragen ask `core.terra.enabled()` first: scenario worlds aren't terra
worlds unless they say so.

World settings are written at creation into the world's `world.mt`:
`creative_mode`, `enable_damage` (copied into the engine's settings when the
menu picks the world, as Luanti's own menu did), and one `tf_*` key per
survival rule; the world code goes into `map_meta.txt` as `mgterra_code`. Systems read their key when they are built. The engine
only accepts those keys (see `worlds.cpp`).

---

## The game

Where we're going. Every content and system decision is checked against this
section; if a change pulls away from it, raise it.

### What we fix in Minecraft

Minecraft's first hours are magic; after that it's a hodge-podge that ends
quickly. The world is scenery (where you settle barely matters). Progression
is one ladder of materials with bigger numbers. Its systems don't feed each
other. Nothing rewards building a town, a stable or a smithy beyond looks.
Distance costs nothing. Mods fill the gaps in pieces, hacking around an engine
they don't own. We own ours, so the pieces can be designed as one game.

What we take from the mod community:
- **TerraFirmaCraft:** the world is the challenge. Geology places ore, seasons
  make storage matter, metallurgy is a real process. Gate on knowledge and the
  world, not grind.
- **GregTech:** a legible tier spine makes a long game coherent. Not its
  tedium: steps must be decisions, never padding.
- **Create:** machines are physical and visible, not humming GUI boxes. Its
  gap is that nothing needs them; ours must be needed.
- **Factorio:** logistics *is* a game once moving things has a cost.
- **Farming, animal and colony mods:** depth from time and care (soil,
  seasons, breeding), and towns that produce and need things.

### Pillars

1. **The world is the game.** Every resource lives where it makes sense:
   ore by geology, crops by climate, animals by biome, power by rivers and
   wind. You can't get everything at home, which creates exploration,
   transport, trade and towns.
2. **Builds exist because they're needed.** A stable because horses sicken in
   winter; a smithy because a forge needs a chimney, an anvil and a quench
   side by side; a granary because food rots. Nothing is built only for looks
   (though everything can look good).
3. **Systems feed each other.** Every output is another system's input. No
   pillar of play stands alone.
4. **Towns are engines.** Settlers (later Axon agents) come for food,
   shelter and work; towns specialise, produce and trade. Later ages need an
   economy, not one player.
5. **Distance is real.** No ender chests. Roads, carts, rails, boats, then
   trains, aircraft and rockets.
6. **Finishing means adventuring.** The last stages need the extremes (poles,
   deep caves, deserts, peaks), then the moon, other planets and other stars.

### Progression: ages

Each age needs the one before it *and* a new place in the world, so progress
pulls you outward.

- **Stone:** knapping, fire, shelter, foraging, the first winter.
- **Bronze:** copper and tin from different places (the first expedition);
  kilns, pottery, storage, early farming.
- **Iron:** bloomery, smithing; animals at work (ploughs, carts, horses).
- **Mechanical:** water wheels on real rivers, windmills where it's windy,
  shafts and gears; the first factories.
- **Industrial:** steam, electricity, rails; logistics becomes the game.
- **Modern:** electronics, automation, chemistry, flight.
- **Space:** rockets, orbits, the moon and planets with resources Earth
  lacks; eventually FTL to other stars, each a new world code.

### Automation is required by kind, not quantity

Tech must be hard without automation but never a grind. Gate tiers on what
hands *can't* do, never on "collect 10,000 of X":
- **rate:** more air, heat or power than a person can supply;
- **continuity:** processes that can't stop (steel, annealing);
- **precision:** tolerances only machines reach; precise machines make more
  precise machines, and that ladder is the tech spine;
- **environment:** clean, sealed, vacuum, extreme heat or cold;
- **scale of energy:** more than one engine gives, hence networks.

Each tier asks for a new capability, not a bigger pile. Recipes stay
learnable without a wiki.

### Mechanics

The physics we build, roughly in the order the ages need them:

1. **Heat**, the keystone: temperatures, conduction, insulation, fuel
   grades, melting and burning points. Feeds survival (hearths, body
   temperature), kilns and metallurgy, and later engines. First.
2. **Mechanical power:** torque and speed from rivers, wind, animals, then
   steam. Early industry has to sit on a river.
3. **Fluids and pressure:** water, steam, oil and molten metal in pipes;
   aqueducts, irrigation, casting; boilers that fail if neglected.
4. **Biology over time:** soil nutrients, rotation, seasons, disease;
   animal genetics, feed, shelter, health.
5. **Logistics:** weight and bulk; carts, roads, rails, boats, cranes.
6. **Structural load:** supports, timbered mines, real bridges. Gentle by
   default.
7. **Atmosphere:** thin air at altitude, bad air in mines (ventilation),
   vacuum in space (sealed habitats, suits).
8. **Electricity:** voltage, current, losses over distance.
9. **Chemistry:** ratios and byproducts (slag, fractions, pollution) that
   force chains.
10. **Orbital mechanics:** Kepler orbits on rails, delta-v, launch windows.
    Space is a simulation, not blocks: ships hand over between planet layers
    and the orbital sim; the sky shows its bodies.
11. **People:** settlers with needs and work.

**The three-question test.** A mechanic earns its place only if it answers
all three: *what does the world provide?* (place matters), *what can't hands
do?* (automation is necessary), *what does it feed?* (one game, not toys).

**Simulate where the effect lives.** Heat, fluids and load are sparse
simulations that run only where something is happening (inside a furnace,
along a pipe network), never a field over the whole world. A deep atmosphere
and weather simulation is a long-term idea, at the coarse grid's scale, not
now.

### Engine primitives (directional)

Ambitious on purpose: this is where we land, built step by step. Each step
ships with the content that needs it.

**Native supplies mechanisms, Lua supplies policy.** The engine models the
physics and assumes no content: it knows resistance, ignition points and
composition, never "copper wire", "coal" or "bronze". Lua builds every piece
of content and every rule. Where a thing goes:
1. per block, per tick, or across many blocks → native;
2. the client must know or predict it → native;
3. a mechanism ("temperature here") is native; a rule ("freezing hurts")
   is Lua;
4. anything every mod would want, framework or not → a native `core.*` API.

**Every primitive has the same four parts:** *properties* on node and item
definitions (any mod declares them, so all blocks take part); *state* the
engine owns, sparse and saved with the map; *Lua verbs* to query, inject
(sources and parameters at runtime) and subscribe to threshold events (Lua
never polls blocks); and native *couplings* between primitives (current
heats wires, burning releases heat, heat raises steam pressure).

**Decision fidelity.** A law is modelled only as deep as it changes what a
player decides or builds. Every law must be predictable without a wiki
(TerraFirmaCraft's depth with none of its opacity), local and sparse, and
budgeted per tick, degrading gracefully.

**Built (sprint 7, `planning/sprint-07-matter.md`):** the substance table,
the first piece of matter. `src/matter` loads `matter/*.json` and gives
each substance's state (solid, liquid, gas, supercritical) at any
temperature and pressure, from its triple and critical points, its 1 atm
boiling or sublimation point and the slope of its melting line. The
main menu's **Matter** screen shows it: a periodic table, structures in
3D, and a phase diagram to drag across, with reference worlds marked.

The catalogue:
1. **Environment fields**, pure and unstored: temperature (climate, season,
   time, altitude, depth), humidity, wind, pressure, sunlight, later rain.
   `core.env.at(pos)`. Trees, crops, windmills and bodies read them. A later
   weather simulation only changes how they're computed.
2. **Heat:** conductivity, capacity, emissivity, thresholds; sparse
   conduction with radiation approximated; hot things glow by black-body
   colour.
3. **States and phases:** substances declare small phase tables (melting
   and boiling over pressure, latent heats). Items and stacks carry
   temperature and phase: a hot ingot cools in your hand.
4. **Rooms:** sealed volumes found by flood fill, each with pressure,
   temperature and gas composition (ideal gas law); openings flow by
   pressure. Fires use oxygen, mines need ventilation, boilers build
   pressure, habitats need airlocks. Per room, never gas per block.
5. **Matter: substances → mixtures → reactions** (see "Matter: one chain
   from the flask to the planet" below). Burning, smelting, alloying,
   curing (concrete strength rising over time), and explosions as fast
   gas-producing reactions spiking room pressure into structural failure.
6. **Networks:** one graph solver with kinds as data: rotation (torque,
   speed), fluid (pressure, flow), electric (voltage, current, I²R heat).
   Content declares conductors and ports; Lua injects parameters
   (`core.network.set`), reads results and handles events (`on_overload`).
7. **Structure:** strength and density; local support checks on place and
   dig; Lua decides what a failure does; strictness per world.
8. **Motion:** rigid bodies via **Jolt Physics** (MIT): block assemblies
   that move and settle, vehicles from physical parts (mass from blocks,
   wheels with suspension, engines with torque curves, hulls as sealed
   rooms so leaks sink ships, wings with lift and drag), gravity as a
   field, drag from air pressure, buoyancy. Needs client authority for
   drivers.
9. **Composite blocks:** any block can be refined into 8³ (maybe 16³)
   micro-voxels, stored only where someone builds them; physics reads the
   micro shape (mass, displacement, drag, lift). Never subdividing the
   whole world.
10. **Shaped terrain:** the generator places slopes, wedges and corners
    where the ground steps, so land is walkable and drivable; players grade
    roads.
11. **Space:** orbits on rails, the sky as its window; later FTL.

### Matter: one chain from the flask to the planet

Everything physical derives from one table and one primitive.

- **Substances** (built, sprint 7): `matter/*.json`, every element's
  identity and the modelled substances with their properties. The engine
  gives a substance's state at any temperature and pressure.
- **Mixtures** (built, sprint 8, `planning/sprint-08-reactions.md`): amounts of substances, plus a temperature and a
  pressure or volume. A mixture's properties come from what's in it, which
  is what makes alloys, composites, solutions, brines and air possible.
  Each step it asks each substance's state, runs the reactions whose
  conditions are met at their rates, and balances energy (reaction heats,
  latent heats). **One primitive at every scale:** a crucible's contents and
  a room's air (seconds to minutes), a lake, an ocean or a planet's
  atmosphere (millions of years). It is the "reservoir" of the modelling
  boundary (`planning/systems-and-bodies.md`).
- **Reactions** (built, sprint 8; the Matter screen's **Reactor** runs them live): `matter/reactions.json`. Each has reactants,
  products, heat released or absorbed, a rate that rises with temperature
  from an activation energy (Arrhenius), and requirements: a state
  (dissolved in liquid water), a catalyst, or an energy input (electricity,
  light). **Element balance is checked when the file loads**, so no reaction
  makes or destroys atoms. Combustion, smelting, electrolysis, rusting,
  calcining, curing, photosynthesis.
- **In the world:** blocks declare what they're made of, and the heat
  primitive plus the engine's states do the rest. Ice melts above its
  melting point at the local pressure, lava cools to basalt, water boils
  cooler on a mountain. Containers and items carry mixtures (copper and tin
  at temperature react into bronze); rooms are gas mixtures (a fire burns
  the room's oxygen).
- **Geology:** the body model is rebuilt on mixtures and reactions over
  geological time. Outgassing chemistry (SO₂ with water makes acid
  clouds), weathering as a reaction (the carbon cycle), and ores from
  chemistry: evaporites (salt left as a sea dries), hydrothermal sulfides,
  placers, and banded iron from life's oxygen rusting the oceans' iron.
  Strata record the history.
- **Life:** metabolism as reactions (photosynthesis, chemosynthesis,
  methanogenesis), emerging where a solvent, an energy source and the
  elements it needs exist. Biomass is a reservoir; the oxygen rise is
  chemistry, not a script. Evolution later (traits, adaptation). Crops and
  animals have chemical and environmental needs, so farming falls out of
  the same rules.
- **The boundary holds:** substances, amounts and rates. No atoms,
  molecular dynamics or fluid simulation. Realistic in outcome, cheap in
  computation.

**Order:** reactions and mixtures (with a menu view to watch them) → heat
and block materials in the world → rooms → the body model on mixtures
(chemistry, weathering, strata, ores) → life as metabolism. The substance
library grows with each step, by what that step needs.

**Geology uses the same ideas at world creation**, deterministic and fixed
point on the coarse grid: ores form by process. Hydrothermal veins near old
intrusions, placers downstream of them in our rivers, coal in buried swamp
basins, oil under sedimentary domes, diamonds in kimberlite pipes from the
deep, salt from dried seas. Mods declare ores by formation process;
terragen supplies the process fields. Prospecting means reading the land.

**Depth:** caves hold the yearly mean temperature; below, a geothermal
gradient (cooling and ventilation, then suits); rock hardens and caverns
need support; a molten deep world far below with exotic minerals, reached
with actively cooled suits; geothermal power late.

**Not doing:** per-block gas or fluid dynamics, finite-element stress,
molecular chemistry, subdividing the whole world.

**Order:** environment fields + properties and states on definitions →
heat and rooms → reactions → geology → structure → networks → composite
blocks and shaped terrain → rigid bodies and vehicles → space.

### Built-ins and content as data

Everyone gets the world; everything above it is a built-in that can be
switched off.

1. **Platform (always on):** engine, terragen and the `terra` generator,
   `core.terra`, the UI channels, and the content registry.
2. **Framework mods:** how content works: materials and tiers, processes,
   crafting, survival needs, heat, power, logistics.
3. **Content packs:** the stuff, an age or a domain at a time (`tf_stone`,
   `tf_bronze`, `tf_farming`, `tf_animals`, ...).

With every `tf_*` gameplay mod off, you're left with a clean world on our
generator to build your own game on. Our packs use only public APIs, so the
API stays honest.

**Adding content is data, not code.** One declaration says what something is,
and the framework derives tools, recipes, tinted textures, icons and world
placement:

```lua
tf.material("bronze", { tier = 2, alloy = { copper = 0.88, tin = 0.12 }, melts = 950, tools = true })
tf.ore("cassiterite", { yields = "tin", rock = { "granite" }, depth = { -60, 20 }, deposit = "vein" })
```

Per-kind behaviour lives in tables, never `if` chains across files. Ores
belong to rock types and rock types come from terragen (geology, phase 5), so
prospecting is a skill.

**Difficulty:** the default is tuned between Minecraft and TerraFirmaCraft
(around Hardy); the rules presets reach either end.

---

## Decided design

### World

- **Finite**, sizes 2k–16k blocks a side. **Wraps east–west; north and south
  end in polar edges** (impassable ice). This gives real latitude: poles,
  equator, seasons that make sense.
- Generated from a **world code**: `TF2.<seed>.<settings>` — the seed plus only
  the land settings that differ from defaults, versioned. (TF1 was sprint 1's
  generator; it is refused, not reinterpreted.) The code carries the
  **land only**; rules travel separately. A code must never silently change
  meaning: a generator change reproduces old codes or refuses them.
- The seed field accepts plain text or a full code. Share copies the code.

### Generation (terragen)

**Built (sprint 1, `planning/sprint-01-terragen.md`):** phases 0–1. terragen
is a pure fixed-point function of the code (value noise periodic east–west
by lattice wrap, CORDIC trig, the solar model); `mg_name = terra` builds
terrain, climate (yearly mean) and biomes from it; `worlds.create` writes
`mg_name`, `mgterra_code` and the engine seed into `map_meta.txt`; the create
screen's preview comes from terragen via `worldgen.preview`. A unit-test
**golden** pins the output — changing generation for an existing code means a
new code version, not editing the golden.

**Built (sprint 2, `planning/sprint-02-water.md`):** phase 2, water. A 256²
coarse grid per code (`terragen/grid.*`, cached and shared across threads):
sea placed by quantile so land matches the Land setting exactly; priority-
flood filling; implicit stream-power erosion; flow and rain-weighted
accumulation; rivers as the top share of drained land (the Rivers setting
picks 0.4–4%); lakes only for basins of real size. `World::ground(x, z)`
gives each column its terrain and water (sea / lake / river) — carved
channels, banks that hold rivers in, lake floors and rims. The preview sends
lake flags and river segments; the map and globe draw them. Water invariants
(drains to the sea, flows downhill, land share) are unit-tested.

**Built (sprint 6, `planning/sprint-06-bodies.md`):** the **body model**
(`terragen/body.*`): every body in a system is a short history stepped from
formation to the system's age. From causes (star type, age and wind; orbit,
mass, where it formed, volatiles, day, life) come its card (kind, air and
gases, temperature, field, seas, plates, tides, composition, gravity) and
its story. Fixed point (`log2`/`exp2`/`pow` in `fixed.*`), calibrated so our
own solar system comes out as itself (unit-tested). Served by
`system.preview`; the create screen's System tab holds causes only
(`ui/src/system/`). Direction: `planning/systems-and-bodies.md`.

**Built (sprint 11, `planning/sprint-11-alloys.md`):** early craft
chemistry as data. Alloys are substances made by reactions that need their
metals molten (bronze, pewter, cast iron, steel by blowing air through cast
iron); wood burns in air or chars to charcoal short of it; sand and soda ash
make glass; lime slakes and sets back to limestone as mortar. Metals carry a
Mohs `hardness`. Reactor presets for each.

**Built (sprint 10, `planning/sprint-10-vegetation.md`):** vegetation.
terragen decides how much can grow on each column and where trees stand
(`World::cover`); `core.terra.cover(minp, maxp)` serves a chunk; species are
content in `tf_world/vegetation.lua` (niche → shape), so mods add species
without engine changes.

**Built (sprint 9, `planning/sprint-09-geology.md`):** geology. Plates on
the grid (boundaries classified per plate pair: converging, spreading,
sliding); mountains follow their collisions (`belt`), with rifts, ridges and
trenches (`relief`); volcanoes on arcs, rifts and hot-spot chains, active ones
with level lava craters; provinces (shield, orogen, arc, rift, basin,
volcanic, ocean floor); folded strata; ores by process; caves by rock (karst,
lava tubes, rare caverns). Rock and ore per block come from terragen
(`World::strata/rock/ore/cave`, 3D fixed noise `value3`); the game supplies
blocks by alias (`mapgen_rock_*`, `mapgen_ore_*`, `mapgen_placer`,
`mapgen_lava_source`). Codes are **TF3**. `core.terra.geology(pos)`;
plates and geology overlays; body interiors (`BodyCard.layers`).

A deterministic **coarse simulation** of the whole finite world, computed at
creation (seconds) and cached in the world folder, then sampled per chunk with
fine detail. One implementation serves the engine *and* the menu preview
(`worldgen.preview` replaces the JS mock).

- **Determinism is a hard requirement:** integer / fixed-point noise for
  anything that decides terrain; floats only for visual-only detail. Same code
  → same world on every machine.
- **Tileable** east–west from the start.
- Takes a **world kind** input (earthlike now; moon and planets later).
- Phases: 0 foundation (module, `mg_name = terra`, coarse grid, engine
  preview) → 1 shape (continents, mountains, coast) → 2 water (depression
  fill, flow accumulation, rivers, stream-power erosion) → 3 climate (from the
  sun model: latitude, altitude, prevailing wind, rain shadows) → 4 vegetation
  (moisture, temperature, slope, altitude → species and density; treelines;
  riparian forest; L-system trees placed by terragen) → 5 geology (strata;
  ores, oil, coal, placer gold from rock history — prospecting is learnable)
  → caves.
- Libraries are fine where they don't threaten determinism.

### The sun

Axial tilt, year length, day length, sun strength (star type), and optionally
orbit shape are **world settings**. One small shared solar model gives, for any
latitude and date, sun elevation, day length and insolation. It drives
terragen's climate, in-game seasons (temperature, snow, leaves, crops, day
length, polar day/night) and the sky. The engine's sky (`sky.cpp`, day/night
ratio) is changed to take its sun path from the model; the server sends the
date. The create menu shows the planet lit by the real sun for a chosen date.

**Built (sprint 3):** `set_sky{ solar = { latitude, declination } }` (a
trailing field of `TOCLIENT_SET_SKY`). The client places the sun, moon and
stars from the real time of day, latitude and declination, and feeds
everything that colours and lights the world (sky colours, glow, light ratio,
shadows) the **apparent time**: the stock time at which the stock sun stands
as high as the real one (`client/solar.*`). `tf_sky` keeps each player's
solar sky current. The engine's clock (time of day and its speed) is
unchanged; time is the same at every longitude.

### Survival

Rules are data, from arcade to realistic (`menu/create/rules.ts`, presets
Arcade / Classic / Hardy / Realistic): hunger, healing, illness, thirst, body
temperature, stamina, difficulty, keep-inventory, day length, weather,
seasons. Creative and survival are both first-class. **Body temperature is the
first system** — it exercises sun → climate → rule → Lua system → state stream
→ HUD. Game Lua will feed the HUD via `player:set_ui(key, value)` (planned).

### Later

- **Planets:** a planet is another finite world (its own code, kind, rules),
  living as a separate high layer of the same map. The moon first, once Earth
  is good. Orbits are numbers driving phases, tides, eclipses and the sky.
  Other planets hold resources Earth doesn't; other stars (FTL) are new world
  codes.
- **Weather:** a coarse atmosphere on the terragen grid (heat, pressure,
  moisture, wind, rain shadows), with extreme events (storms, blizzards,
  droughts, floods through the river network) spawned where conditions allow.
  Deferred.
- **Multiplayer** is mostly free (Luanti is always client+server). Anything we
  add to the protocol means our client and server only — bump the protocol so
  mismatches are refused cleanly.

---

## Code standards (our code)

- **Fail loudly.** No swallowed errors, no `?? fallback` hiding a value that
  must exist, no silent defaults. Invalid state throws. Expected refusals are
  structured (`Refused`, `WorldCodeError`) and shown to the player.
- **Validate at boundaries, trust inside.** The bridge, file I/O, world
  settings and inventory slots are checked where they enter.
- **No `as` casts** to silence a type mismatch — fix the shape or add the
  honest primitive (typed switches over discriminated unions).
- **Data over branches.** Variants live in tables that every decision reads
  (screens, options, presets, tabs, world settings whitelist).
- **One concern per module**; factories/composables in TS, classes in C++.
  Orchestrators wire; leaves work.
- **Comments say why**, at the density of the surrounding code.
- **Unwired things say so** — in code (throw "not wired yet") and in the UI
  (Notice: "preview only", "saved for later"). Never pretend.

## Licensing

- The engine is LGPL: our engine changes stay public. The game's own code and
  art can be whatever we choose.
- **Never build on Mineclonia/VoxeLibre** (GPL) for the product.
- **Assets** (images, music, sounds) need a redistributable licence (CC0, CC-BY,
  OFL…) and an entry in `assets/CREDITS.md` before they're committed. Nothing
  resembling Minecraft characters or branding.
- Fonts are OFL (bundled via @fontsource).

## Status

Done: CEF shell, bridge, main menu (home, worlds, create in three steps —
Start (enough to play), World (body tree · viewer · inspector for the
selected body, Earth's land as its Surface), Rules — relief globe and map, world codes, name generator, settings),
HUD (hotbar, hearts, breath, crosshair), item icons, pause screen, inventory
(crafting, move/drop, shift-move), component gallery, trees, and **terragen
phases 0–2**: worlds are generated from their code — land, climate, rivers,
lakes, carved valleys — and the preview is the world.

The create viewer has the system list (Sun ▸ Earth ▸ Moon), overlays
(terrain, biomes, elevation, temperature, moisture), a date scrubber and
star / tilt / year settings, all drawn from terragen (the mock now serves
only the gallery).

**The sun on the live world (sprint 3, `planning/sprint-03-seasons.md`):**
a calendar from the engine's clock, the sky following the real sun per
player, and `core.terra` for game Lua.

**The Lab (sprint 4, `planning/sprint-04-lab.md`):** scenarios, the panel,
the UI channel, `bun check:lab`. Its first run caught water that had never
been a liquid, and lakes with stepped surfaces.

Next: the game. First `planning/game-design.md`, which turns "The game" into
the ages, their unlocks and the first content tables. Then the framework
skeleton (content registry, materials and tiers) and a stone-age vertical
slice: knapping, fire and **heat**, shelter, foraging, and body temperature on
`core.terra.climate` (needs the `player:set_ui` HUD channel). A first winter
you can lose. Geology (strata, ores) follows, because bronze needs it.
Known items: very large basins become high sediment plains (fine for now);
small streams below grid resolution don't exist yet.

Queued UI work: make the shared Settings screen production-grade. Expose the
relevant engine settings in clear pages (video, graphics, audio, controls,
accessibility, and advanced as needed), with graphics presets, keybinding
edit/reset/conflict handling, and any missing kit components represented in
the gallery. Keep the page and validated engine settings bridge in sync.

Default art: `tf_world` uses selected Good Vibes textures (Acaitart, CC BY 4.0)
with source revision and attribution in `assets/CREDITS.md`. The user still
needs to review the look in-game and share screenshots for any colour or water
tuning. Add new blocks in the same visual style and record new asset sources.
