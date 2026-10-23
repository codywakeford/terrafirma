# Animated orbital menu scene

## Source of truth

Generate a deterministic `SceneSpec` when the menu opens: planet world code, star, date/lighting epoch, moon description, camera composition, and a scene seed. The first version may select a curated range of valid planet codes and a simple seeded rocky Moon; a configurable Moon becomes its own world code when that model exists. Keep one selected scene stable during a menu visit. Save or cache generated data so returning to the menu does not cause a visible replacement or generation stall.

Sample the planet through `terragen`'s existing C++ preview/world path. Generate geometry and upload it once when the scene is selected. A change of scene needs new data; ordinary rotation and camera motion need only transform matrices. Seasonal climate can be chosen at the scene's date, but the planet must not request a new world preview each frame.

## Rendering approach

Render the 3D scene in the native menu loop before the transparent CEF UI is composited. The browser is already capable of transparency; the current opaque CSS menu background would be removed or made translucent for this screen. This keeps continuous full-screen animation out of CEF's offscreen paint and texture upload path. The world creation globe can remain a smaller WebGL preview, while the native menu renderer shares its *terrain data and visual rules* rather than its WebGL objects.

Build a coarse voxel globe mesh from the preview's elevation and biome fields. A cube-sphere grid avoids a pile-up of tiny cells at the poles. Each cell samples the world by latitude/longitude. Quantized radial heights, square-ish top faces, and exposed sides between height steps make the surface visibly block-built; a separate ocean shell can stay smoother while coastlines remain stepped. Avoid trying to display every gameplay block from orbit. Choose the visible cell scale for the shot and add mesh resolution levels only if camera motion actually needs them.

Render passes: sparse star field; distant star and small bodies; planet solid surface and water; clustered cloud shell; thin atmospheric scattering/rim; restrained glare. Use one star direction and colour for every lit body. Give the Moon a compatible coarse voxel mesh and deterministic crater/rock pattern until moon generation supplies its own fields.

## Animation and composition

- Keep the camera near the blue-hour flyby framing, with the left side reserved for menu controls. The camera may drift a few degrees, but the bright limb and Moon should remain outside the control zone.
- Rotate the planet continuously at an art-directed, slow menu rate. Let clouds move independently. The star remains fixed within each scene and the terminator changes as the planet turns. Move the Moon subtly, preserving its light phase.
- Use a continuous time source and transforms, so the scene does not visibly restart or jump when UI panels change. Reduced-motion mode freezes at a composed frame.
- Generate a different system on a later menu visit or through an explicit future control, not randomly during navigation between Play, Settings and world creation.

## First implementation slice

1. Native camera and one generated voxel planet, fixed star lighting, fixed Moon placeholder, UI overlaid at left.
2. Slow rotation and camera drift; stable scene selection and loading/cache path.
3. Atmosphere, clustered clouds, water response and Moon surface. Profile frame time and menu paint/upload cost in the real client.
4. Replace Moon placeholder with configurable moon generation and system data when those models are ready.

The current globe is an orthographic WebGL2 single-body preview. It already proves the engine fields can displace a sphere and light it, but a perspective multi-body cinematic scene and native menu renderer require new scene code.

## Current first pass

`src/shell/orbit.*` now draws a native 3D menu scene using a `terragen` preview from a curated random world code. It includes stepped terrain and coasts, biome colours, height walls, a simple water glint, clustered cloud patches, atmosphere and star glows, a lit procedural rocky Moon, sparse stars, and slow motion. The menu CSS is transparent over it and the Home controls occupy the dark left side. The world code stays stable for the process.

The current Moon is a seeded visual placeholder. The cloud shape and water glint are also visual effects, not yet outputs of a weather or ocean simulation. The user needs to review the result in the game client before we tune camera framing, contrast, materials, and performance; no gameplay scene was used as the renderer for this orbital shot.
