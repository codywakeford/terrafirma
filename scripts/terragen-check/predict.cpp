// Standalone terragen (no engine): what each probed column should be, and
// which columns to probe. Built and run by scripts/terragen-check.sh.
//   predict <code> <points-out>   → PREDICT lines on stdout, x z per line in <points-out>
//   predict --terra <code> <probed> → what core.terra should have said, for
//                                     each TERRA line's column and clock
// Columns: a coarse grid over the world, plus river channels and lake centres.
#include "terragen/calendar.h"
#include "terragen/sun.h"
#include "terragen/geology.h"
#include "terragen/world.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace terragen;

static double real(fix v)
{
	return (double)v / ONE;
}

static double degrees(fix radians)
{
	return real(radians) * 180.0 / M_PI;
}

// Mirrors src/script/lua_api/l_terra.cpp from the clock the probe read.
static int terra(const char *code, const char *probed)
{
	World w(WorldCode::decode(code));
	FILE *in = std::fopen(probed, "r");
	char line[512];
	while (std::fgets(line, sizeof line, in)) {
		const char *at = std::strstr(line, "TERRA ");
		int x, z;
		unsigned day_count;
		double tod;
		if (!at || std::sscanf(at, "TERRA %d %d %u %lf", &x, &z, &day_count, &tod) != 4)
			continue;
		fix hour = (fix)((float)tod * ONE);
		Calendar c = calendar(w.code().year, day_count, hour);
		fix v = w.v(z), latitude = World::latitude(v);
		fix decl = declination(w.code().tilt, c.date);
		Column column = w.column(w.u(x), v, w.sunlight(v, c.date));
		Ground gr = w.ground(x, z, w.sunlight(v, std::nullopt));
		std::string province = PROVINCE_NAMES[(int)w.strata(x, z, gr).province];
		std::replace(province.begin(), province.end(), ' ', '_');
		std::printf("TERRA %d %d %u %d %d %.6f %.6f %.6f %.6f %.6f %s %s %.6f %s\n", x, z, day_count,
				c.year + 1, c.day + 1, real(c.date), degrees(latitude), degrees(decl),
				degrees(elevation(latitude, decl, hour)), real(dayShare(latitude, decl)),
				SEASON_NAMES[(int)season(c.date, latitude)], BIOME_NAMES[(int)column.biome],
				real(w.temperature(x, z, c.date, hour)), province.c_str());
	}
	std::fclose(in);
	return 0;
}

int main(int argc, char **argv)
{
	if (std::strcmp(argv[1], "--terra") == 0)
		return terra(argv[2], argv[3]);
	World w(WorldCode::decode(argv[1]));
	const Grid &g = w.grid();
	const int water_level = 1; // the engine's default
	int32_t size = w.code().size, cell = size / GRID;

	std::vector<std::pair<int, int>> points;
	for (int i = -2; i <= 2; i++)
		for (int j = -2; j <= 2; j++)
			points.push_back({ i * size / 6, j * size / 6 });
	// the centres of some river and lake cells: a river runs through its cell's centre
	int rivers = 0, lakes = 0;
	for (int32_t k = 0; k < GRID * GRID; k += 97) {
		int x = -size / 2 + (k % GRID) * cell + cell / 2, z = size / 2 - ((k / GRID) * cell + cell / 2);
		if (g.river(k) && rivers < 12) { points.push_back({ x, z }); rivers++; }
		else if (g.lake[k] && lakes < 6) { points.push_back({ x, z }); lakes++; }
	}

	// soil under each biome (games/terrafirma/mods/tf_world/biomes.lua): top
	// (or riverbed under water) + filler, leaving out plain stone, which the
	// generator doesn't lay: there the bedrock shows
	auto soil = [](Biome b, bool bed) {
		switch (b) {
		case Biome::Deep: case Biome::SeaIce: return bed ? 4 : 3;
		case Biome::Ocean: return bed ? 5 : 4;
		case Biome::Beach: return 4;
		case Biome::Desert: return 8;
		case Biome::Jungle: case Biome::Monsoon: return bed ? 6 : 5;
		case Biome::Tundra: case Biome::Mediterranean: return bed ? 4 : 3;
		case Biome::Rock: return bed ? 2 : 0;
		case Biome::Snow: return bed ? 2 : 1;
		default: return bed ? 5 : 4;
		}
	};

	// a couple of active volcanoes: the crater (lava) and a flank
	int volcanoes = 0;
	for (const Volcano &v : g.volcanoes) {
		if (!v.active || volcanoes == 2)
			continue;
		points.push_back({ v.x, v.z });
		points.push_back({ v.x + v.radius / 2, v.z });
		volcanoes++;
	}

	FILE *out = std::fopen(argv[2], "w");
	const char *kinds[] = { "dry", "sea", "lake", "river", "lava" };
	for (auto [x, z] : points) {
		std::fprintf(out, "%d %d\n", x, z);
		Ground gr = w.ground(x, z, w.sunlight(w.v(z), std::nullopt));
		int terrain = water_level + gr.terrain;
		int water = gr.kind == Water::None ? -9999 : water_level + gr.water;
		// the block 20 below the surface, as the generator decides it
		bool bed = (gr.kind == Water::River || gr.kind == Water::Lake) && gr.water > gr.terrain;
		int depth = 20, below = depth - soil(gr.column.biome, bed);
		Strata st = w.strata(x, z, gr);
		Rock rock = w.rock(st, below, x, terrain - depth, z);
		Cave cave = w.cave(st, rock, depth, x, terrain - depth, z, gr.kind != Water::None && gr.kind != Water::Lava);
		Ore ore = w.ore(st, rock, below, x, terrain - depth, z);
		std::string deep = cave == Cave::Air ? "air" : cave == Cave::Water ? "tf_world:water_source"
				: cave == Cave::Lava ? "tf_world:lava_source"
				: ore != Ore::None ? std::string("tf_world:ore_") + ORE_NAMES[(int)ore]
				: std::string("tf_world:") + ROCK_NAMES[(int)rock];
		std::printf("PREDICT %d %d %d %d %s %s %s\n", x, z, terrain, water, kinds[(int)gr.kind], BIOME_NAMES[(int)gr.column.biome], deep.c_str());
	}
	std::fclose(out);
}
