# The planet as blocks

How Terra Firma lays ordinary voxel blocks over a round planet. This is the
reference for the design. It replaces the cube-sphere-with-transition-bands
decision in `planning/sprint-29-seamless-globe.md`.

**Layout v2 (2026-10-27): segments, caps and a core**, proved in Cody's 2D
seam simulation, replaces layout v1, the onion of latitude bands and radial
shells (section 10 says why). The engine runs v2 since 2026-10-28 (section 11).

---

## 1. The stance

You cannot cover a sphere with equal, square, gravity-aligned cubes.
Every layout gives something up. The cube-sphere gives up block shape across
whole faces: its own baseline (sprint 29) has corners of 72°–108° across the
central 75% of every face.

We stop trying to remove the distortion. **We choose where it lives.** The
plan:

- The overwhelming majority of blocks are, to a player, ordinary cubes.
- The unavoidable irregularity is pushed into **seams**: narrow, predictable
  places that follow the planet's own geography (latitude, the poles, the
  core).
- Seams are buildable. Blocks there can be slightly odd, and straight lines
  can jog or turn there, but the world is physically continuous.
  Position, velocity, collision and gravity are never distorted.

What a player should feel:

1. **Gravity is straight down a column, near enough.** Inside a region a
   block's "up" is within a fraction of a degree of true up, and the
   regions most people live in are exactly aligned down their middles.
2. **Ordinary blocks are imperceptibly different from a cube.** Edge
   lengths are within a tolerance of 1 m (target ~1%, i.e. 1 cm), corners
   are right angles.
3. **Seams are predictable.** They run along a few fixed meridians and two
   polar circles, the same from the surface to the core. A player can learn
   where they are, and digging down never crosses one.
4. **Seams are rare where people live.** Large uninterrupted building areas
   at ordinary latitudes, and the worst seams concentrated where the planet
   naturally has extremes (poles, core).

## 2. The layout: segments, caps and a core

The planet is made of a handful of large regions. Each is **one Cartesian
grid**: integer columns, rows and layers, the same from the surface to as
deep as the region goes.

| Region | Where | Count |
|---|---|---|
| **Segment** | A wedge of the planet between two meridians, from the cap circle in the north to the one in the south, surface down to the core | `N` round the planet (an orange's segments) |
| **Cap** | Poleward of the cap latitude, surface down to the core | 2 |
| **Core** | Inside the core radius | 1 |

### A segment's grid

A segment's grid is laid on its **centre line**: the meridian down its
middle, at the equator. Its axes are east, north and up (toward the
planet's centre) there, and a cell is one block in each.

On its own, a Cartesian grid tilts against gravity away from its centre
line: at a segment's edge "up" for the grid leans by half the segment's
width. So each grid is **rounded**: its cells are placed partway between the
flat grid and the same grid wrapped onto the sphere. For a cell `(i, j, k)`
(east, north, layer):

- **flat:** `F = (R + k)·up + i·east + j·north`, a polygon's face;
- **round:** `S`, the same column and row wrapped onto the sphere: `i` and
  `j` blocks of arc along the surface, `k` layers out along the radius;
- **placed:** in the direction of `(1 − ρ)·F + ρ·S`, at exactly `R + k`
  from the centre, with **rounding** `ρ` near 1 (~0.95).

Every layer is therefore a true sphere. *(Changed 2026-10-28.)* Placing the
cell at the blend itself, as first written, lifts a layer off the sphere by
about `(1 − ρ)·x²/2R` at `x` blocks from a grid's centre line. Two segments
lift equally at their shared meridian, so they still meet level, but a
segment meets a cap's circle on its centre line, where it isn't lifted,
while the cap's edge is: a ledge of 0.25 blocks on the dev world, 113 on
Kerbin and 1,211 (a 1.2 km cliff) on Earth. Taking only the direction from
the blend keeps everything rounding buys and makes every seam level by
construction.

What rounding buys:

- **Up points (almost) at the core.** Columns lean by only `(1 − ρ)` of what
  a flat grid's would.
- **The surface is round.** Layers are spheres (above); rounding sets only
  how cells spread across them.
- **Blocks keep their width with depth.** A perfectly round grid (`ρ = 1`)
  narrows toward the core: its blocks squash east–west as you dig, by
  `(R − depth)/R`. A flat grid doesn't narrow at all. Stopping short of
  round keeps blocks nearly their surface width all the way down.

### Trimming

Because a rounded grid narrows more slowly than the wedge it fills,
neighbouring grids **overlap** along the meridian between them, more the
deeper you go. The overlap is **trimmed**: every place belongs to exactly
one region, and a grid's cells outside its own region are removed.

- The boundary between two segments is the meridian halfway between their
  centre lines, a plane through the spin axis. *(Provisional: the 2D
  simulation's rule decides; a cell is kept by the grid holding most of
  it.)*
- Cells the boundary cuts through are **cut cells**: odd shapes, the only
  irregular blocks inside the segments' depth. They follow the boundary, a
  thin staircase down each seam.
- Everything else is an **uncut cell**: a near-perfect block, aligned to its
  neighbours above, below and beside it.

The trimming is the mechanism. Rounded grids would otherwise need squashed
blocks to fit the narrowing wedge. Trimming lets them keep their width, and
puts the whole cost of the fit into cut cells at the seams.

### Caps

Every segment's seam is a meridian, and all meridians meet at the poles. So
poleward of the **cap latitude** a cap takes over: its own grid, centred on
the pole (axes: two horizontals and up), rounded the same way and trimmed
against the segments along the cap circle. The pole is an ordinary building
site; no seam reaches it.

### The core

The segments' seams also converge going down: every one meets every other
at the centre. So inside the **core radius** the core takes over: one plain
Cartesian grid (unrounded), trimmed against the segments and caps along a
sphere. Its blocks don't follow gravity, but deep inside a planet gravity
falls roughly linearly toward zero. What fills the core is per-planet
content (magma, a hollow, an underworld), not a geometry decision.

### Parameters

| Parameter | What it sets | Starting value |
|---|---|---|
| `N` | segments round the planet | 8 |
| cap latitude | where the caps take over | to measure |
| core radius | where the core takes over | to measure |
| `ρ` | rounding of segments and caps | ~0.95 |
| trim rule | who owns an overlapped place | provisional (above) |

All are data: a planet's layout is a list of regions with their kind, frame,
extent and rounding, and the engine reads it. The planetary viewer
(sprint 31) shows any layout and its measures, so these get tuned by
looking and measuring, in 3D.

### Storage

Each region's grid is a box in the map. Its trimmed cells are seam: nothing
is stored there. A segment is a wide box (millions of blocks east–west on a
large planet), split into storage pieces that are the same grid on both
sides, so not seams.

## 3. The seams

| Seam | Shape | What happens there |
|---|---|---|
| **Segment seam** | A half-plane through the spin axis, along a meridian, from cap to cap and surface to core | Two rounded grids meet at a small angle. Cut cells both sides; a block can face several across it. The fit worsens with depth, which is what trimming absorbs. |
| **Cap circle** | A cone round the spin axis at the cap latitude, surface to core | The cap's grid meets each segment's: cut cells along it. A line crossing it turns by the angle between the two grids. |
| **Core boundary** | A sphere | Every region meets the core's plain grid: cut cells, and the core's blocks don't follow gravity. |

There are **no seams going down** outside the core: digging from the
surface stays in one grid until the core. Seams never meet many at a
point: the caps take the meridians' meeting at the poles, the core their
meeting at the centre.

## 4. Straight lines

The railway test is the gameplay metric: *how far can you build straight
before the grid forces a visible change?*

- **Inside a region**, a row or column is a straight line of the region's
  grid, rounded: it follows the surface's curve, not a ring or a meridian.
  It runs unbroken across the whole region: a segment is thousands of
  kilometres east–west on a large planet.
- **At a segment seam** a line crossing it turns by the small angle
  between the two grids, and may jog by part of a block.
- **Into a cap** a line turns by the angle between the segment's and the
  cap's grids.

What's measured: distance between turns, worst turn, worst jog.

## 5. Planet size

A layout has a fixed number of regions, so the regions grow with the
planet and seams get further apart. What changes with size is the angle
two grids meet at a seam (set by `N`, not radius) and the sagitta of a
rounded face (which grows with radius at fixed `N` and `ρ`).

- **Large (Kerbin ~600 km to Earth ~6,371 km):** seams thousands of
  kilometres apart. Possibly more segments, to keep the seam angle and
  sagitta down.
- **Medium and small:** fewer segments. Where a single region would do (a
  moon, an asteroid) is a measurement, not a guess.
- **Asteroids / built objects:** a single Cartesian grid with gravity toward
  its centre (or none). Not attempted here.

## 6. The contract

The engine sees the layout through one interface (`terragen::Atlas`) and
never knows which layout is underneath:

- `locate(point) → cell`: which block contains a point in planet space.
- `corners(cell)`: the block's physical geometry (8 corners).
- `neighbours(cell, face) → cells`: possibly more than one, at T-junctions
  and cut edges. Lighting, liquids, collision and pathfinding step through
  this, never through integer arithmetic on coordinates.
- `quality(cell)`: stretch, skew, gravity alignment, retained volume, seams
  touched.

A cell address is `(region, i, j, k)`: a region's own grid coordinates.
Segments, caps and the core are each plain integer grids. Inside one, everything the engine does today (16³
mapblocks, meshing, the Lua API) still applies; only stepping across a
region boundary is special.

**Engine consequences** (for the integration sprint, not the lab):

- Addressing becomes per-region (plus storage segments). That removes the
  ±31,000-node map limit and keeps positions small enough for float
  precision anywhere on any planet.
- **Rendering is layout-blind already:** each block's mesh is bent onto the
  planet by its grid's map at build time, and every block is drawn with the
  eye's one transform (sprint 30). A new layout needs no rendering change.
- **Storage keeps one rule:** a plain step (`p + dir`) from a stored cell
  is a true neighbour or seam, never another cell. Every uniform grid (a
  band or cylinder in a shell, a cap in a shell, the core) is its own
  region, walled by a map block of seam, so code that doesn't know the
  atlas can be wrong only by stopping, never by reaching the wrong place.
  Under v2 that's one box per region.
  Crossing between regions goes through the atlas (`step`, `see`,
  `carry`), as the cube net's did through `Planet::step`.
- The existing seam stepping (`Planet::step` / `halo` / `turn`) generalizes
  to `neighbours` that are not 1:1.
- Each region is meshed in its own frame and placed by its transform.
  `sphere_lens` goes away (done: removed with the cube net, 2026-10-28).
- Sky, weather and clouds are being replaced regardless. Orbit physics stays
  in its own double-precision space.
- A saved world's mapping is versioned. Old saves stay on the cube-sphere
  until migrated explicitly.

## 7. Measures

What the lab reports, and the provisional targets. These are engineering
targets, not proven limits of perception. We tune them by looking.

| Measure | Target |
|---|---|
| Uncut cells | > 99% of the surface; tracked through depth |
| Cut cells | count per seam, per layer |
| Block edge stretch, uncut cells | within ±0.5% |
| Corner skew (grid skew), uncut cells | < 0.5° |
| Gravity misalignment, outside the core | < 0.5° (bounded by `(1 − ρ)` × half a segment) |
| Surface sagitta | worst departure from the sphere, in blocks |
| Retained volume of cut blocks | report slivers (< 10%); they become merge candidates |
| Rails | distance between turns, worst turn, worst jog |
| Cap entry | worst turn |

## 8. Open questions

1. **Trim rule.** The halfway meridian, kept by majority, until the 2D
   simulation's rule is ported and the two are compared on a slice.
2. **Segment count `N`** against seam angle and sagitta, by planet size.
3. **Rounding `ρ`.** 0.95 from the simulation; tune in 3D. Whether caps
   and segments want the same value.
4. **Cap latitude and core radius.** Bigger caps mean shorter seams but a
   longer cap circle; a bigger core means seams stop sooner but more blocks
   off gravity.
5. **Cut cells.** Exact clipped shapes, and merging slivers into a
   neighbour, as for v1.
6. **Which way the grids turn.** A cap's axes against the segments' (which
   longitudes enter it straight).
7. **Small bodies.** Below what radius one region does the whole body.

## 9. The lab

The v1 lab (`scripts/atlas-lab/`) was removed with the move to v2 on
2026-10-28. The planetary viewer (sprint 31) replaces it: any layout in
game, from orbit to the core. Its measurements of v1 are kept below.

### First measurements of layout v1 (2026-10-27)

1% band and shell tolerance, cap at 85°, 64 layers each way. Surface layer:

| Radius | Regular blocks | Shells | Seams at 45° | Cap radius | Cut cells per cap layer (slivers) |
|---|---|---|---|---|---|
| 300 b | 48.8% | 54 | every 2 b | 26 b | 212 (23%) |
| 1 km | 80.6% | 14 | 10 b | 88 b | 708 (21%) |
| 3 km | 93.6% | 5 | 29 b | 262 b | 2.1k (21%) |
| 10 km | 98.2% | 3 | 100 b | 873 b | 7.0k (19%) |
| 60 km | 99.70% | 1 | 602 b | 5.2 km | 42k (20%) |
| 600 km | 99.97% | 1 | 6.0 km | 52 km | 420k (20%) |
| 6,371 km | 99.997% | 1 | 64 km | 556 km | 4.5M (20%) |

Measured from real corners: band blocks have skew < 1e-8° and gravity
error 0; cap blocks shrink by at most 0.76% with 0.22° skew. North–south
rails jog at most 0.50 blocks at each latitude seam. On these defaults,
roughly 10 km is where the layout becomes comfortable. Below about 3 km it
needs different settings or a different layout.

v1 limits: cut cells are flagged and their retained fraction measured, but
drawn as whole blocks rather than clipped polyhedra. Neighbours are found by
sampling each face.

## 10. Layout v1: the onion (superseded)

Layout v1 (sprint 30) covered the planet with **latitude bands** (rings of
equal column count, a whole column dropped at each band seam), **radial
shells** (each with its own rows and bands, re-gridded at each shell
floor), gnomonic **caps** poleward of 85°, and a plain **core**. It is
what the engine runs today (`terragen::Atlas`, `planet::Atlas`).

It kept blocks within 1% everywhere and gravity exactly radial, but:

- **Digging down crossed grids.** Every shell floor re-gridded rows and
  columns, so a shaft passed misaligned blocks every ~26 layers (on a
  1.3 km planet), and the seams there showed as cracks and slits until the
  renderer learned to look across them.
- **Many seams.** Hundreds of band seams per hemisphere, re-made in every
  shell, and on small planets a region for every row near the caps: five
  times the blocks to send at the cap edge.
- **Seams everywhere people dig.** Shell floors are horizontal seams under
  every building site.

Layout v2 trades exact radial gravity for **one grid from the surface to
the core** in every region, with all irregularity in a few vertical seams.

## 11. Moving to layout v2

The engine depends on the layout only through the contract (section 6):
`locate`, `corners`, `neighbours`, `quality`, and in storage `cell`,
`node`, `step`, `see`, `carry`, `frame`. What changes:

- **Rewritten:** the geometry (`terragen::Atlas`): regions as data,
  rounding, trimming, locating a point, the quality measures. And the
  storage packing (`planet::Atlas`): one box per region, which is simpler
  than v1's per-band, per-shell packing.
- **Simplified or removed:** shells and shell floors; band column counts and
  the cylinder staircase; the per-row regions near the caps, and the
  thin-strip handling for them in choosing blocks to send.
- **Unchanged:** rendering (bent meshes, per-block transforms, the
  across-seam fill-in and rebuilds), the far view (layout-blind),
  collision and movement across seams (`carry`, `see`), the headless
  render test (it compares against the geometry, whatever it is).
- **Regenerated:** worlds. A v1 world's mapping is versioned
  (`atlas-v1`); v2 is `atlas-v2`, and old atlas worlds stay v1 or are
  thrown away (they are dev worlds).

### Where the move stands (2026-10-28)

Done, under test:

- **Geometry:** `TestAtlas` (11): round trips, tiling, block quality,
  symmetric neighbours, one grid down to the core, and layers as spheres up
  to Earth's size.
- **Storage:** `TestPlanetAtlas` (12), including steps poleward into the
  caps and steps never landing in another cell of their own grid.
- **Headless:**
  - the world check (154,634) and the debug sphere (90,120);
  - the seam check (505): walkers, water and sand across segment seams and
    into both caps.
- **Cut cells** (where a grid's territory ends inside a cell) are clipped to
  their territory, as a box in their own axes (`planet::Atlas::extent`), for
  collision and for drawing alike. Two grids' cut cells over one place no
  longer both stand there.
- **Slivers merged into a cell:** a step from that cell walks on through the
  sliver to the next grid (`terragen::Atlas::beyond`).
- **Costs:**
  - A seam step costs 0.8 µs the first time, then 0.4 µs (kept).
  - A step in a block clear of every seam costs nothing extra
    (`planet::Atlas::interior`).
  - Liquid flow across seams: 23–44 ms a transform, against 2.6 s before.
