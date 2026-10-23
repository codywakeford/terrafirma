# Main menu: orbital flyby art direction

These are concept stills for the main menu scene. They establish **framing and light placement only**. Their photographic terrain is not the game's visual target, and they are not assets to display as the final animated background.

| Direction | Still | Read |
| --- | --- | --- |
| **01 — Blue hour flyby** | [View](01-blue-hour-flyby.png) | Closest camera, strongest sense of passing the planet. Atmosphere and terrain remain visible. Best starting composition. |
| **02 — Eclipse rim** | [View](02-eclipse-rim.png) | Strong silhouette and coherent Sun/Moon lighting. More dramatic, but too much of the world is dark for the primary menu shot. |
| **03 — Blue world portrait** | [View](03-blue-world-portrait.png) | Best read of the planet and landscape. Feels more like a static globe portrait than a flyby. |

## Chosen direction

Build from **01**, borrowing the narrow, controlled rim light of **02**. Keep the planet cropped along the right and bottom edges. Its atmosphere should be legible as multiple thin layers, with the strongest cyan at the tangent. Show generated, visibly voxel terrain on the lit side and allow the dark side to fall into deep navy. The Sun should just clear the far limb: small bright core, subtle warm scatter, no broad flare. A small rocky voxel Moon hangs above the horizon, lit from the same direction. Other planets can appear as distant points, never as a line of large spheres.

## Visual language

- The planet is a **block-built world wrapped into a globe**: stepped coastline, discrete terrain levels, readable square surface patches and cliff faces. Its large-scale continents, biomes and heights come from `terragen`, the same source as the world creation preview.
- At orbital scale, individual gameplay blocks would be subpixel. Use an art-directed coarse representation of those blocks. Preserve real world shapes while exaggerating the size of visible block clusters and relief.
- Light the geometry with a coherent star direction, a day/night terminator, soft shadow, restrained specular water and atmospheric scattering. The *effects* can be smooth while the solid planet remains visibly constructed from blocks.
- Clouds should also have an intentional stepped or clustered silhouette. Do not paint photographic clouds over the voxel terrain.
- Moon and other solid bodies should follow the same shape language. The star is an emissive disc and glow.

## Framing for the menu

- Reference canvas: 16:9. Keep the left ~40% quiet and dark for the title and navigation. Place the brightest part of the Sun in the upper-right quarter, away from controls.
- On narrower screens, crop the planet farther right and preserve the control zone. Do not shrink the planet into a centred full globe.
- Treat stars as sparse depth cues. No orbital tracks, labels, spacecraft, city lights, purple nebulae, or diagram elements.
- The present home screen is centred. Move its content into the protected left zone when this scene is implemented; this art direction does not change the running UI yet.

## Lighting and motion target

- Palette: near-black and deep navy space; cool teal atmosphere; muted natural terrain; small warm ivory Sun.
- Lighting: a shared Sun direction for surface, clouds, atmosphere, and Moon phase. Keep fill light weak enough to preserve the terminator without losing all landscape detail.
- Motion: slow camera translation past the limb, slight planetary rotation, and even slower cloud movement. The scene should feel alive when watched for ten seconds but almost still at a glance. Avoid a visible loop cut.
- If physically exact distances make the Moon or other planets unreadable, adjust the composition deliberately. Keep light and phase relationships coherent.

## Concept prompts

All three stills were generated with the built-in image generation tool as **stylized-concept** images for a 16:9 game menu background. Each prompt requested a physically inspired 3D orbital view, a large Earth-like world to the right, a small rocky Moon, controlled Sun lighting, dark negative space on the left, and no text or UI.

1. **Blue hour flyby:** camera close to a cropped planetary horizon; visible natural land, clouds, thin turquoise atmosphere; Sun emerging behind the far upper-right limb; Moon high in the middle distance; serene charcoal, navy, teal, and amber palette.
2. **Eclipse rim:** camera close to a mostly shadowed world; narrow cyan atmospheric rim, small ivory Sun corona, Moon above the limb; minimal light on the surface and very dark left-side space.
3. **Blue world portrait:** camera somewhat farther away; more of the planet's circular limb and landscape visible; soft dawn lighting, clouds and mountain relief, Moon above-left, dark left-side space.

The generated landforms and Moon placements are composition references. The eventual scene should use the game's own world surface and celestial data where practical.
