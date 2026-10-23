// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "terrain.h"
#include "map.h"
#include "mapgen/treegen.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

using namespace terragen;

namespace far {

// How far a tree's crown can reach from its trunk, in blocks: trees standing
// this far outside a level-0 tile are grown too, for what they lean into it.
static constexpr s32 REACH = 12;
// How tall a tree can grow above its base.
static constexpr s32 TALLEST = 48;
// The engine's map chunks: 5 blocks of 16, from −32 (chunksize, the
// default). The game plants trees per chunk, and a tree is clipped to its
// chunk and the block-wide shell around it.
static constexpr s32 CHUNK = 80, CHUNK_ORIGIN = -32, SHELL = 16;

static s32 chunkMin(s32 v)
{
	return (s32)std::floor((double)(v - CHUNK_ORIGIN) / CHUNK) * CHUNK + CHUNK_ORIGIN;
}

float radius(const WorldCode &code)
{
	return RADIUS_PER_SIZE * (float)code.size;
}

Generator::Generator(const Horizon &horizon) :
	m_horizon(horizon),
	m_world(std::make_unique<World>(WorldCode::decode(horizon.code))),
	m_columns(std::make_unique<terra::Columns>(*m_world, horizon.blocks, horizon.water_level))
{
	// each species' typical crown, from growing a few of its trees
	for (const terra::Species &s : m_horizon.species) {
		treegen::TreeDef def = terra::treeDef(s);
		s32 lows = 0, highs = 0, grown = 0;
		for (s32 k = 0; k < 8; k++) {
			MMVManip vm(nullptr);
			v3s16 base(k * 37, 1, k * 53);
			vm.addArea(VoxelArea(base - v3s16(REACH, 1, REACH), base + v3s16(REACH, TALLEST, REACH)));
			const VoxelArea &a = vm.m_area;
			for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
			for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
			for (s32 x = a.MinEdge.X; x <= a.MaxEdge.X; x++)
				vm.m_data[a.index(x, y, z)] = MapNode(y < base.Y ? m_horizon.blocks.stone : CONTENT_AIR);
			treegen::make_ltree(vm, base, def);
			s32 low = TALLEST, high = -1;
			for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
			for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
			for (s32 x = a.MinEdge.X; x <= a.MaxEdge.X; x++) {
				content_t c = vm.m_data[a.index(x, y, z)].getContent();
				if (c == s.leaves)
					low = std::min(low, y - base.Y);
				if (c == s.leaves || c == s.trunk)
					high = std::max(high, y - base.Y);
			}
			if (high < 0)
				continue;
			lows += std::min(low, high);
			highs += high;
			grown++;
		}
		m_crowns.push_back(grown ? Crown{ lows / grown, highs / grown } : Crown{ 0, -1 });
	}
}

Generator::~Generator() = default;

static Cell cellOf(const terra::Columns &columns, const terra::Column &c, s32 x, s32 z)
{
	const terra::Blocks &b = columns.blocks();
	Cell cell{};
	if (c.beyond) {
		cell.ground = columns.waterLevel() + POLAR_WALL;
		cell.top = cell.filler = cell.rock = MapNode(b.ice);
		cell.top_end = cell.filler_end = cell.ground;
		cell.water = cell.ground;
		return cell;
	}
	auto solid = [&](s32 y) {
		MapNode n = columns.node(c, x, y, z);
		content_t id = n.getContent();
		// a cave or spring opening at a cliff: show the rock it's cut in
		return id == CONTENT_AIR || id == b.water || id == b.lava ? MapNode(b.stone) : n;
	};
	cell.ground = c.height;
	cell.top = solid(c.height);
	cell.top_end = c.top_end;
	cell.filler_end = c.filler_end;
	cell.filler = c.top_end > c.filler_end ? solid(c.top_end) : cell.top;
	cell.rock = solid(std::min(c.filler_end, c.height - c.soil));
	cell.water = c.height;
	if (c.water > c.height) {
		cell.water = c.water;
		cell.liquid = columns.node(c, x, c.water, z);
	}
	return cell;
}

Cell Generator::column(s32 x, s32 z) const
{
	terra::Column c = m_columns->at(x, z, m_columns->sunlight(z));
	return cellOf(*m_columns, c, x, z);
}

const terra::Species *Generator::speciesAt(const terra::Column &c, s32 x, s32 z) const
{
	if (c.beyond)
		return nullptr;
	Cover cover = m_world->cover(x, z, c.ground);
	content_t ground = m_columns->node(c, x, c.height, z).getContent();
	return terra::choose(m_horizon.species, terra::spotAt(c.ground, cover), ground);
}

Tile Generator::build(const TileKey &key) const
{
	Tile tile;
	tile.key = key;
	tile.cells.resize((size_t)(TILE + 2) * (TILE + 2));
	const s32 cell = key.cell(), x0 = key.x * key.span(), z0 = key.z * key.span();
	for (s32 j = -1; j <= TILE; j++) {
		// each cell shows its centre column
		s32 z = z0 + j * cell + cell / 2;
		fix sun = m_columns->sunlight(z);
		for (s32 i = -1; i <= TILE; i++) {
			s32 x = x0 + i * cell + cell / 2;
			terra::Column c = m_columns->at(x, z, sun);
			Cell &out = tile.at(i, j) = cellOf(*m_columns, c, x, z);
			if (key.level == 0 || c.beyond || c.water > c.height)
				continue;
			// a coarse cell: its species' crown wherever the forest is dense enough
			const terra::Species *s = speciesAt(c, x, z);
			if (!s)
				continue;
			const Crown &crown = m_crowns[s - m_horizon.species.data()];
			Cover cover = m_world->cover(x, z, c.ground);
			if (crown.high < 0 || (float)(cover.roll & 0xffff) / 65536.0f >= (float)cover.density / ONE)
				continue;
			out.canopy = { c.height + 1 + crown.low, c.height + 1 + crown.high, MapNode(s->leaves) };
			if (crown.low > 0)
				out.trunk = { c.height + 1, c.height + crown.low, MapNode(s->trunk) };
		}
	}
	if (key.level == 0)
		growExact(tile);
	return tile;
}

void Generator::growExact(Tile &tile) const
{
	const s32 x0 = tile.key.x * TILE, z0 = tile.key.z * TILE;
	// the area the tile shows, border included, and where trees reaching into it stand
	const s32 ax = x0 - 1, az = z0 - 1, aw = TILE + 2;
	const s32 rx = ax - REACH, rz = az - REACH, rw = aw + 2 * REACH;
	std::vector<terra::Column> region((size_t)rw * rw);
	for (s32 j = 0; j < rw; j++) {
		fix sun = m_columns->sunlight(rz + j);
		for (s32 i = 0; i < rw; i++)
			region[(size_t)j * rw + i] = m_columns->at(rx + i, rz + j, sun);
	}
	auto at = [&](s32 x, s32 z) -> const terra::Column & { return region[(size_t)(z - rz) * rw + (x - rx)]; };

	// The trees, by the chunk they're planted in, in the order the game
	// plants them (tf_world/vegetation.lua: z outer, x inner): where crowns
	// overlap, the first planted keeps the blocks, as in the world.
	struct Planted
	{
		v3s16 base;
		const terra::Species *species;
	};
	std::map<std::tuple<s32, s32, s32>, std::vector<Planted>> chunks;
	for (s32 z = rz; z < rz + rw; z++)
	for (s32 x = rx; x < rx + rw; x++) {
		const terra::Column &c = at(x, z);
		if (c.beyond)
			continue;
		// where the game plants: a tree spot, dry ground, and not the chunk's
		// top layer, which its planting skips
		s32 y = c.height;
		if (!m_world->cover(x, z, c.ground).tree || c.water > c.height || y - chunkMin(y) == CHUNK - 1)
			continue;
		const terra::Species *s = speciesAt(c, x, z);
		if (s)
			chunks[{ chunkMin(x), chunkMin(y), chunkMin(z) }].push_back({ v3s16(x, y + 1, z), s });
	}

	for (const auto &[chunk, trees] : chunks) {
		auto [cx, cy, cz] = chunk;
		// the chunk's voxels and the block-wide shell around them, where they show in the tile
		v3s16 lo(std::max(cx - SHELL, ax), cy - SHELL, std::max(cz - SHELL, az));
		v3s16 hi(std::min(cx + CHUNK - 1 + SHELL, ax + aw - 1), cy + CHUNK - 1 + SHELL,
				std::min(cz + CHUNK - 1 + SHELL, az + aw - 1));
		if (lo.X > hi.X || lo.Z > hi.Z)
			continue;
		MMVManip vm(nullptr);
		vm.addArea(VoxelArea(lo, hi));
		// the ground the trees grow among
		for (s32 vz = lo.Z; vz <= hi.Z; vz++)
		for (s32 vx = lo.X; vx <= hi.X; vx++) {
			const terra::Column &g = at(vx, vz);
			for (s32 vy = lo.Y; vy <= hi.Y; vy++)
				vm.m_data[vm.m_area.index(vx, vy, vz)] = MapNode(vy <= g.height ? m_horizon.blocks.stone
						: vy <= g.water ? m_horizon.blocks.water : CONTENT_AIR);
		}
		for (const Planted &t : trees)
			treegen::make_ltree(vm, t.base, terra::treeDef(*t.species));

		// each column's tree blocks: the run at the top, and the one under it
		auto tree = [&](s32 vx, s32 vy, s32 vz) {
			content_t id = vm.m_data[vm.m_area.index(vx, vy, vz)].getContent();
			for (const terra::Species &s : m_horizon.species)
				if (id == s.leaves || id == s.trunk)
					return true;
			return false;
		};
		for (s32 vz = lo.Z; vz <= hi.Z; vz++)
		for (s32 vx = lo.X; vx <= hi.X; vx++) {
			auto run = [&](s32 &vy) {
				Run r;
				while (vy >= lo.Y && !tree(vx, vy, vz))
					vy--;
				if (vy < lo.Y)
					return r;
				r = { vy, vy, vm.m_data[vm.m_area.index(vx, vy, vz)] };
				while (vy - 1 >= lo.Y && tree(vx, vy - 1, vz))
					vy--;
				r.low = vy--;
				return r;
			};
			s32 vy = hi.Y;
			Run top = run(vy);
			if (top.empty())
				continue;
			Run below = run(vy);
			// a column under trees of two chunks: the higher crown shows
			Cell &cell = tile.at(vx - x0, vz - z0);
			if (cell.canopy.empty() || top.high > cell.canopy.high) {
				cell.canopy = top;
				cell.trunk = below;
			}
		}
	}
}

} // namespace far
