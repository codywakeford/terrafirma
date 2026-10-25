// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "geology.h"
#include "grid.h"
#include "noise.h"
#include "world.h"
#include <algorithm>
#include <queue>

namespace terragen {

const char *const PROVINCE_NAMES[(int)Province::Count] = {
	"shield", "orogen", "arc", "rift", "basin", "volcanic", "ocean floor",
};
const char *const ROCK_NAMES[(int)Rock::Count] = {
	"granite", "gneiss", "slate", "basalt", "andesite", "tuff", "obsidian", "sandstone", "shale", "limestone",
};
const char *const ORE_NAMES[(int)Ore::Count] = {
	"none", "copper", "tin", "lead", "gold", "iron", "magnetite", "coal", "salt", "sulfur",
};

namespace {

// Noise streams for geology (offsets from the world's seed hash).
enum GeoStream : uint32_t {
	PLATE = 101, PLATE_WARP = 103, VOLCANO = 107, HOTSPOT = 109, SEDIMENT = 113, FOLD = 127,
	PLUTON = 131, VEIN = 137, METAL = 139, MAGNETITE = 149, BANDED = 151, CAVE_A = 157, CAVE_B = 163,
	TUBE = 167, CAVERN = 173, SULFUR = 179, PHASE = 181, INLAND_BASIN = 191,
};

uint32_t mix(uint32_t a, uint32_t b, uint32_t seed)
{
	uint32_t h = seed ^ (a * 0x27d4eb2du) ^ (b * 0x165667b1u);
	h = (h ^ (h >> 15)) * 0x2c1b3c6du;
	h = (h ^ (h >> 12)) * 0x297a2d39u;
	return h ^ (h >> 15);
}

// 0..1 from a hash
fix unit(uint32_t h)
{
	return (fix)(h & (ONE - 1));
}

fix sq(fix v)
{
	return mul(v, v);
}

// 1 − d/reach, squared, for d < reach: how strongly a boundary is felt
fix falloff(int32_t d, int32_t reach)
{
	if (d >= reach)
		return 0;
	return sq(ONE - ratio(d, reach));
}

const int DX4[4] = { 1, -1, 0, 0 };
const int DZ4[4] = { 0, 0, 1, -1 };

int32_t step(int32_t i, int dx, int dz)
{
	int32_t cx = i % GRID + dx, cz = i / GRID + dz;
	if (cz < 0 || cz >= GRID)
		return -1;
	return Grid::index((cx + GRID) % GRID, cz);
}

} // namespace

Tectonics tectonics(const WorldCode &c, const std::vector<fix> &mass, fix sea_offset)
{
	Tectonics t;
	size_t n = (size_t)GRID * GRID;
	uint32_t seed = c.seed_hash + PLATE;

	int count = 6 + c.continents;
	for (int k = 0; k < count; k++) {
		Plate p;
		p.u = unit(mix(k, 1, seed));
		p.v = ratio(12, 100) + mul(unit(mix(k, 2, seed)), ratio(76, 100));
		fix s, co;
		sincos(mul(unit(mix(k, 3, seed)), TWO_PI), s, co);
		fix speed = HALF + unit(mix(k, 4, seed)) / 2;
		p.du = mul(speed, co);
		p.dv = mul(speed, s);
		int32_t cx = std::min(GRID - 1, floorInt(p.u * GRID)), cz = std::min(GRID - 1, floorInt(p.v * GRID));
		p.oceanic = mass[Grid::index(cx, cz)] < sea_offset;
		t.plates.push_back(p);
	}

	// each cell to its nearest plate centre, through a warp so the edges wander
	t.plate.resize(n);
	for (int32_t cz = 0; cz < GRID; cz++) {
		fix v = ratio(2 * cz + 1, 2 * GRID);
		for (int32_t cx = 0; cx < GRID; cx++) {
			fix u = ratio(2 * cx + 1, 2 * GRID);
			fix wu = u + mul(fbm(u, v, c.seed_hash + PLATE_WARP, 4, 3, HALF), ratio(6, 100));
			fix wv = v + mul(fbm(u, v, c.seed_hash + PLATE_WARP + 1, 4, 3, HALF), ratio(6, 100));
			int64_t best = INT64_MAX;
			for (int k = 0; k < count; k++) {
				fix du = fabsx(fraction(wu - t.plates[k].u));
				du = std::min<fix>(du, ONE - du);
				fix dv = wv - t.plates[k].v;
				int64_t d = (int64_t)du * du + (int64_t)dv * dv;
				if (d < best) {
					best = d;
					t.plate[Grid::index(cx, cz)] = (uint8_t)k;
				}
			}
		}
	}

	// boundaries: how each pair of plates moves across the line between their
	// centres. Classified per pair, so a whole boundary has one kind (per
	// cell edge, a diagonal boundary flickered between kinds)
	std::vector<Boundary> pair_kind((size_t)count * count, Boundary::None);
	std::vector<Side> pair_side((size_t)count * count, Side::None);
	for (int a = 0; a < count; a++)
		for (int b = 0; b < count; b++) {
			if (a == b)
				continue;
			const Plate &pa = t.plates[a], &pb = t.plates[b];
			fix nu = pb.u - pa.u;
			if (nu > HALF)
				nu -= ONE;
			if (nu < -HALF)
				nu += ONE;
			fix nv = pb.v - pa.v;
			fix len = std::max<fix>(sqrt(mul(nu, nu) + mul(nv, nv)), 1);
			fix closing = mul(pa.du - pb.du, div(nu, len)) + mul(pa.dv - pb.dv, div(nv, len));
			Boundary kind = closing > ratio(3, 10) ? Boundary::Convergent
					: closing < -ratio(3, 10) ? Boundary::Divergent : Boundary::Transform;
			pair_kind[(size_t)a * count + b] = kind;
			if (kind == Boundary::Convergent) // the denser plate sinks: ocean under continent; between oceans, the lower index
				pair_side[(size_t)a * count + b] = pa.oceanic == pb.oceanic
						? (pa.oceanic ? (a < b ? Side::Overriding : Side::Subducting) : Side::Collision)
						: (pa.oceanic ? Side::Subducting : Side::Overriding);
		}
	t.boundary.assign(n, Boundary::None);
	t.side.assign(n, Side::None);
	t.distance.assign(n, 255);
	std::queue<int32_t> frontier;
	for (int32_t i = 0; i < (int32_t)n; i++) {
		for (int k = 0; k < 4; k++) {
			int32_t j = step(i, DX4[k], DZ4[k]);
			if (j < 0 || t.plate[j] == t.plate[i])
				continue;
			size_t pair = (size_t)t.plate[i] * count + t.plate[j];
			Boundary kind = pair_kind[pair];
			if (t.boundary[i] == Boundary::None || kind == Boundary::Convergent ||
					(kind == Boundary::Divergent && t.boundary[i] == Boundary::Transform)) {
				t.boundary[i] = kind;
				t.side[i] = pair_side[pair];
			}
		}
		if (t.boundary[i] != Boundary::None) {
			t.distance[i] = 0;
			frontier.push(i);
		}
	}
	// distance to the nearest boundary of the cell's own plate, carrying its kind and side inward
	while (!frontier.empty()) {
		int32_t i = frontier.front();
		frontier.pop();
		if (t.distance[i] >= 40)
			continue;
		for (int dz = -1; dz <= 1; dz++)
			for (int dx = -1; dx <= 1; dx++) {
				int32_t j = step(i, dx, dz);
				if (j < 0 || t.plate[j] != t.plate[i] || t.distance[j] <= t.distance[i] + 1)
					continue;
				t.distance[j] = t.distance[i] + 1;
				t.boundary[j] = t.boundary[i];
				t.side[j] = t.side[i];
				frontier.push(j);
			}
	}

	// what each boundary does to the land
	t.belt.resize(n);
	t.relief.assign(n, 0);
	for (size_t i = 0; i < n; i++) {
		int32_t d = t.distance[i];
		bool sea = mass[i] < sea_offset;
		fix belt = ratio(18, 100);
		switch (t.boundary[i]) {
		case Boundary::Convergent:
			if (t.side[i] == Side::Collision)
				belt = std::max(belt, falloff(d, 8));                 // two continents: the highest ranges
			else if (t.side[i] == Side::Overriding)
				belt = std::max(belt, falloff(d, 6));                 // the arc above a sinking plate
			else if (sea && d < 2)
				t.relief[i] = -mul(ratio(22, 100), falloff(d, 2));     // the trench
			break;
		case Boundary::Divergent:
			if (sea)
				t.relief[i] = mul(ratio(10, 100), falloff(d, 4));       // a mid-ocean ridge
			else
				t.relief[i] = -mul(ratio(5, 100), falloff(d, 3));       // a rift valley
			break;
		default:
			break;
		}
		t.belt[i] = belt;
	}
	return t;
}

std::vector<Volcano> volcanoes(const WorldCode &c, const Grid &g)
{
	const Tectonics &t = g.tectonics;
	std::vector<Volcano> out;
	int32_t cell = c.size / GRID, half = c.size / 2;
	uint32_t seed = c.seed_hash + VOLCANO;
	auto place = [&](int32_t i, VolcanoKind kind, int age, uint32_t h) {
		if (out.size() >= 250)
			return;
		int32_t cx = i % GRID, cz = i / GRID;
		Volcano v;
		v.kind = kind;
		v.x = -half + cx * cell + (int32_t)(h % (uint32_t)cell);
		v.z = half - (cz * cell + (int32_t)((h >> 8) % (uint32_t)cell));
		uint32_t a = mix(h, 11, seed), b = mix(h, 13, seed);
		switch (kind) {
		case VolcanoKind::Arc: // steep cones
			v.radius = std::clamp<int32_t>(cell * 2 + (int32_t)(a % (uint32_t)(cell * 2 + 1)), 40, 150);
			v.height = 60 + (int32_t)(b % 90);
			v.active = (a >> 12) % 100 < 40;
			break;
		case VolcanoKind::Rift: // low fissure cones
			v.radius = std::clamp<int32_t>(cell + (int32_t)(a % (uint32_t)(cell + 1)), 30, 90);
			v.height = 30 + (int32_t)(b % 40);
			v.active = (a >> 12) % 100 < 30;
			break;
		case VolcanoKind::Hotspot: // broad shields, older ones worn down
			v.radius = std::clamp<int32_t>(cell * 3 + (int32_t)(a % (uint32_t)(cell * 2 + 1)), 60, 200);
			v.height = (50 + (int32_t)(b % 70)) * (5 - age) / 5;
			v.active = age == 0;
			break;
		}
		v.crater = std::max(5, v.radius / 7);
		fix e = g.eroded[i];
		v.base = e >= 0 ? floorInt(e * LAND_SCALE) : floorInt(e * SEA_SCALE);
		out.push_back(v);
	};

	for (int32_t i = 0; i < GRID * GRID; i++) {
		int32_t cz = i / GRID;
		if (cz < GRID / 12 || cz > GRID - GRID / 12)
			continue;
		uint32_t h = mix(i, 1, seed);
		bool land = g.eroded[i] >= 0;
		if (t.boundary[i] == Boundary::Convergent && t.side[i] == Side::Overriding && t.distance[i] == 3 && h % 9 == 0)
			place(i, VolcanoKind::Arc, 0, h);
		else if (t.boundary[i] == Boundary::Divergent && t.distance[i] == 0 && land && h % 16 == 0)
			place(i, VolcanoKind::Rift, 0, h);
	}

	// hot spots: plumes from deep down, leaving a chain behind as the plate drifts over them
	int spots = 2 + (int)(c.seed_hash % 3);
	for (int k = 0; k < spots; k++) {
		fix u = unit(mix(k, 1, c.seed_hash + HOTSPOT));
		fix v = ratio(15, 100) + mul(unit(mix(k, 2, c.seed_hash + HOTSPOT)), ratio(70, 100));
		int32_t cx = std::min(GRID - 1, floorInt(u * GRID)), cz = std::min(GRID - 1, floorInt(v * GRID));
		const Plate &p = t.plates[t.plate[Grid::index(cx, cz)]];
		fix len = std::max<fix>(sqrt(sq(p.du) + sq(p.dv)), ratio(1, 10));
		for (int age = 0; age < 4; age++) {
			int32_t ox = roundInt(div(-p.du, len) * 4 * age), oz = roundInt(div(-p.dv, len) * 4 * age);
			int32_t z = cz + oz;
			if (z < GRID / 12 || z > GRID - GRID / 12)
				break;
			place(Grid::index(((cx + ox) % GRID + GRID) % GRID, z), VolcanoKind::Hotspot, age, mix(k, 7 + age, c.seed_hash + HOTSPOT));
		}
	}
	return out;
}

std::vector<Province> provinces(const WorldCode &c, const Grid &g)
{
	const Tectonics &t = g.tectonics;
	size_t n = (size_t)GRID * GRID;
	std::vector<Province> out(n);
	for (size_t i = 0; i < n; i++) {
		int32_t d = t.distance[i];
		fix e = g.eroded[i];
		if (e < 0) {
			out[i] = t.boundary[i] == Boundary::Divergent && d <= 2 ? Province::Rift : Province::OceanFloor;
			continue;
		}
		fix u = ratio(2 * (int32_t)(i % GRID) + 1, 2 * GRID), v = ratio(2 * (int32_t)(i / GRID) + 1, 2 * GRID);
		if (t.boundary[i] == Boundary::Convergent && d <= 4)
			out[i] = t.side[i] == Side::Overriding ? Province::Arc : Province::Orogen;
		else if (t.boundary[i] == Boundary::Divergent && d <= 3)
			out[i] = Province::Rift;
		else if (e < ratio(12, 100) || (e < ratio(22, 100) && g.rain((int32_t)i) > ONE) ||
				// broad sagging basins inside plates, where old seas and swamps laid down their beds
				(t.distance[i] > 8 && value(u, v, c.seed_hash + INLAND_BASIN, 4) > ratio(30, 100)))
			out[i] = Province::Basin;
		else
			out[i] = Province::Shield;
	}
	// lava fields around every volcano
	int32_t cell = c.size / GRID, half = c.size / 2;
	for (const Volcano &v : g.volcanoes) {
		int32_t vcx = (v.x + half) / cell, vcz = (half - v.z) / cell;
		int32_t reach = (v.radius * 3 / 2) / cell + 1;
		for (int32_t dz = -reach; dz <= reach; dz++)
			for (int32_t dx = -reach; dx <= reach; dx++) {
				int32_t cz = vcz + dz;
				if (cz < 0 || cz >= GRID || dx * dx + dz * dz > reach * reach)
					continue;
				out[Grid::index(((vcx + dx) % GRID + GRID) % GRID, cz)] = Province::Volcanic;
			}
	}
	return out;
}

// ---- a column's geology ---------------------------------------------------

namespace {

// a hash of a block, for scattered things (sulfur crystals)
uint32_t blockHash(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
	return mix((uint32_t)x, mix((uint32_t)y, (uint32_t)z, seed), seed);
}

// position in the repeating sequence of sedimentary beds (36 blocks: sand, mud, lime)
int32_t bed(int32_t depth, int32_t phase)
{
	return ((depth + phase) % 36 + 36) % 36;
}

} // namespace

Strata World::strata(int32_t x, int32_t z, const Ground &ground) const
{
	const Grid &g = *m_grid;
	const Tectonics &t = g.tectonics;
	fix u = this->u(x), v = this->v(z);
	int32_t i = cell(u, v);
	Strata s;
	s.province = g.province[i];
	int32_t size = m_code.size;
	fix lumpy = value(u, v, m_code.seed_hash + SEDIMENT, size / 300); // −1..1
	fix e = ground.column.elevation;

	switch (s.province) {
	case Province::Basin: {
		// deepest in the lowest basins: up to ~120 blocks of sediment (inland
		// basins on high ground keep at least 30)
		fix low = clamp(div(ratio(18, 100) - e, ratio(18, 100)), 0, ONE);
		s.sediment = 40 + floorInt(mul(toFix(70), low)) + floorInt(mul(toFix(12), lumpy));
		s.fold = 4;
		break;
	}
	case Province::OceanFloor: s.sediment = 6 + floorInt(mul(toFix(6), lumpy)); s.fold = 2; break;
	case Province::Shield: s.sediment = std::max(0, floorInt(mul(toFix(6), lumpy))); s.fold = 3; break;
	case Province::Orogen: s.sediment = std::max(0, floorInt(mul(toFix(10), lumpy))); s.fold = 20; break;
	case Province::Arc: s.sediment = std::max(0, floorInt(mul(toFix(5), lumpy))); s.fold = 10; break;
	default: s.sediment = std::max(0, floorInt(mul(toFix(5), lumpy))); s.fold = 6; break;
	}
	// beds rise and fall across the land: the folds, broad and gentle away from mountains
	s.tilt = floorInt(mul(toFix(s.fold), value(u, v, m_code.seed_hash + FOLD, std::max(8, size / 400))));
	s.phase = (int32_t)((m_code.seed_hash >> 4) % 36) + floorInt(mul(toFix(10), value(u, v, m_code.seed_hash + PHASE, 6)));
	s.pluton = (s.province == Province::Orogen || s.province == Province::Arc || s.province == Province::Shield) &&
			value(u, v, m_code.seed_hash + PLUTON, std::max(8, size / 700)) > ratio(35, 100);
	s.humid = ground.column.wet > ratio(15, 100);
	s.arid = ground.column.wet < -ratio(20, 100);
	s.placer = s.province == Province::Orogen || s.province == Province::Arc ||
			(t.boundary[i] == Boundary::Convergent && t.distance[i] <= 6);
	s.lava_province = s.province == Province::Volcanic || s.province == Province::Rift || s.province == Province::Arc;

	// the nearest active crater's rim
	s.crater = 1 << 20;
	for (uint32_t k = g.volcano_start[i]; k < g.volcano_start[i + 1]; k++) {
		const Volcano &vo = g.volcanoes[g.volcano_list[k]];
		if (!vo.active)
			continue;
		int32_t dx = x - vo.x;
		dx = ((dx + size / 2) % size + size) % size - size / 2;
		int32_t dz = z - vo.z;
		int32_t d = floorInt(sqrtWide64(((int64_t)dx * dx + (int64_t)dz * dz) << FRAC));
		s.crater = std::min(s.crater, std::abs(d - vo.crater));
	}
	return s;
}

Rock World::rock(const Strata &s, int32_t depth, int32_t x, int32_t y, int32_t z) const
{
	if (s.crater < 8 && depth < 6)
		return Rock::Obsidian; // glass where lava met the air at an active rim
	int32_t d = depth + s.tilt;
	if (s.province == Province::Orogen) // crumpled: beds folded on themselves
		d += floorInt(mul(toFix(6), value3(((x % m_code.size) + m_code.size) % m_code.size, y, z,
				m_code.seed_hash + FOLD, 24, m_code.size)));
	if (d < s.sediment) {
		int32_t b = bed(d, s.phase);
		if (s.arid) // deserts lay down sand
			return b < 18 ? Rock::Sandstone : b < 26 ? Rock::Shale : Rock::Limestone;
		if (s.humid || s.province == Province::OceanFloor) // warm shallow seas lay down lime
			return b < 8 ? Rock::Sandstone : b < 18 ? Rock::Shale : Rock::Limestone;
		return b < 12 ? Rock::Sandstone : b < 22 ? Rock::Shale : Rock::Limestone;
	}
	int32_t under = d - s.sediment;
	switch (s.province) {
	case Province::Shield: return s.pluton ? Rock::Granite : Rock::Gneiss;
	case Province::Orogen: return under < 28 ? Rock::Slate : s.pluton ? Rock::Granite : Rock::Gneiss;
	case Province::Arc: return under < 30 ? (under % 11 < 2 ? Rock::Tuff : Rock::Andesite) : Rock::Granite;
	case Province::Volcanic:
	case Province::Rift: return under < 70 ? (under % 13 < 2 ? Rock::Tuff : Rock::Basalt) : Rock::Granite;
	case Province::Basin: return Rock::Granite;
	case Province::OceanFloor: return Rock::Basalt;
	default: return Rock::Granite;
	}
}

Ore World::ore(const Strata &s, Rock rock, int32_t depth, int32_t x, int32_t y, int32_t z) const
{
	if (depth < 3)
		return Ore::None;
	int32_t size = m_code.size, wx = ((x % size) + size) % size;
	fix scale = HALF + m_code.ores; // the Ore richness setting: half to one and a half

	// sulfur crystals around an active vent
	if (s.crater < 12 && depth < 5 && blockHash(x, y, z, m_code.seed_hash + SULFUR) % 5 == 0)
		return Ore::Sulfur;

	// hydrothermal veins: hot water from the granite below left metals in cracks
	if ((s.province == Province::Orogen || s.province == Province::Arc || s.pluton) && depth > 5) {
		fix n = value3(wx, y, z, m_code.seed_hash + VEIN, 28, size);
		if (fabsx(n) < mul(ratio(3, 100), scale)) {
			fix r = value(u(x), v(z), m_code.seed_hash + METAL, std::max(8, size / 300));
			if (r < -ratio(35, 100))
				return rock == Rock::Granite ? Ore::Tin : Ore::Copper;
			if (r < ratio(25, 100))
				return Ore::Copper;
			if (r < ratio(55, 100))
				return Ore::Lead;
			return depth > 18 ? Ore::Gold : Ore::Copper;
		}
	}
	// magnetite pods in basalt
	if (rock == Rock::Basalt && value3(wx, y, z, m_code.seed_hash + MAGNETITE, 10, size) > ONE - mul(ratio(16, 100), scale))
		return Ore::Magnetite;
	// banded iron: old sea floors in the shields, rusted by life's first oxygen
	if (s.province == Province::Shield && depth >= 8 && depth < 22 && (depth + s.phase) % 4 < 2 &&
			value3(wx, y, z, m_code.seed_hash + BANDED, 48, size) > ratio(5, 100))
		return Ore::Iron;
	bool sedimentary = rock == Rock::Sandstone || rock == Rock::Shale || rock == Rock::Limestone;
	if (sedimentary) {
		int32_t b = bed(depth + s.tilt, s.phase);
		// coal: swamp forests buried in the mud of humid basins
		if (rock == Rock::Shale && s.humid && s.sediment > 20 && b >= 14 && b < 16)
			return Ore::Coal;
		// salt: seas that dried in arid basins
		if (s.arid && s.province == Province::Basin && b >= 4 && b < 7)
			return Ore::Salt;
	}
	return Ore::None;
}

Cave World::cave(const Strata &s, Rock rock, int32_t depth, int32_t x, int32_t y, int32_t z, bool water_above) const
{
	fix amount = m_code.caves;
	if (amount < ratio(1, 64) || depth < 4 || (water_above && depth < 14))
		return Cave::None;
	int32_t size = m_code.size, wx = ((x % size) + size) % size;
	fix scale = HALF + amount;

	// tunnels where two noise sheets cross; water dissolves limestone into networks
	fix width = mul(rock == Rock::Limestone ? ratio(7, 100) : ratio(28, 1000), scale);
	fix a = value3(wx, y, z, m_code.seed_hash + CAVE_A, 20, size);
	if (fabsx(a) < width && fabsx(value3(wx, y, z, m_code.seed_hash + CAVE_B, 20, size)) < width)
		return Cave::Air;

	// lava tubes: flat tunnels just under basalt flows near volcanoes
	if (rock == Rock::Basalt && depth < 16 && (s.province == Province::Volcanic || s.province == Province::Rift) &&
			fabsx(value3(wx, y * 3, z, m_code.seed_hash + TUBE, 24, size)) < ratio(5, 100) &&
			fabsx(value3(wx, y * 2, z, m_code.seed_hash + TUBE + 1, 30, size)) < ratio(12, 100))
		return Cave::Air;

	// deep caverns, their floors flooded: water, or lava under volcanic ground
	if (depth > 60 && value3(wx, y, z, m_code.seed_hash + CAVERN, 56, size) > ratio(86, 100) - mul(amount, ratio(8, 100))) {
		if (s.lava_province && y < -90)
			return Cave::Lava;
		return y < -50 ? Cave::Water : Cave::Air;
	}
	return Cave::None;
}

} // namespace terragen
