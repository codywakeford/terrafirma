# Asset credits

## Terra Firma default block textures

Good Vibes by Acaitart, licensed under [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).

Source: [Phyronnaz/VoxelAssets, GoodVibes](https://github.com/Phyronnaz/VoxelAssets/tree/f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224/GoodVibes), revision `f1bb6fb20e36a47b8dacbdf23c4a10b994b6f224`.

Terra Firma copies selected original PNG files with `tf_goodvibes_` prefixes and uses them for its block faces. The PNG artwork is unchanged. Dry grass and tundra apply colour tints at runtime. Animated water uses the source pack's water texture. A copy of the source licence is in `games/terrafirma/mods/tf_world/GOODVIBES-LICENSE.txt`.

Since sprint 9 the textures are fetched by `scripts/assets/goodvibes.py` from the same pinned revision. Most are copied unchanged (rocks, ores, lava, clay, trees, leaves, plants, from the pack's `minecraft` and `biomesoplenty` folders); the animated lava is scaled down. Some are **derived** from the pack's own art (changes made, as CC BY 4.0 requires noting):

- basalt, gneiss, slate, shale, limestone, salt: the pack's andesite, granite, stone and diorite, retinted;
- copper, tin, lead, magnetite and sulfur ores: the pack's iron or gold ore specks recoloured, on its stone (magnetite on retinted andesite);
- gold-bearing gravel: the gold ore's specks set into the pack's gravel.

Several leaf and plant textures are greyscale in the pack (meant to be tinted per biome); the game multiplies them by a green at runtime.

