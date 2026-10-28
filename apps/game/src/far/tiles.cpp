// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/tiles.h"
#include "far/mesh.h"
#include "constants.h"
#include "exceptions.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

namespace far {

// Tiles put on the GPU per update, at most.
static constexpr int INSTALLS_PER_FRAME = 6;
// A tile not wanted for this many updates is dropped.
static constexpr u32 KEEP_FRAMES = 600;

/*
	The workers: build tiles and their meshes off the main thread, nearest
	first. Each update says which tiles are wanted (want()); keys being
	built or waiting to be taken aren't asked for twice.
*/
class Workers
{
public:
	struct Result
	{
		TileKey key;
		Mesh mesh;
	};

	Workers(const planet::Atlas &atlas, const Horizon &horizon, const Colours &colours, const Globe &globe) :
		m_atlas(atlas), m_horizon(horizon), m_colours(colours), m_globe(globe)
	{
		unsigned n = std::clamp(std::thread::hardware_concurrency() / 4, 1u, 3u);
		for (unsigned i = 0; i < n; i++)
			m_threads.emplace_back([this] { run(); });
	}

	~Workers()
	{
		{
			std::lock_guard lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		for (std::thread &t : m_threads)
			t.join();
	}

	// the tiles wanted, nearest first; replaces what was asked before
	void want(const std::vector<std::pair<float, TileKey>> &keys)
	{
		{
			std::lock_guard lock(m_mutex);
			m_queue.clear();
			for (const auto &k : keys)
				if (!m_busy.count(id(k.second)))
					m_queue.push_back(k.second);
		}
		m_wake.notify_all();
	}

	std::optional<Result> take()
	{
		std::lock_guard lock(m_mutex);
		if (!m_error.empty())
			throw BaseException("far view: " + m_error);
		if (m_results.empty())
			return std::nullopt;
		Result r = std::move(m_results.front());
		m_results.pop_front();
		m_busy.erase(id(r.key));
		return r;
	}

private:
	static std::tuple<s32, s32, s32, s32> id(const TileKey &k) { return { k.level, k.x, k.z, k.face }; }

	void run()
	{
		{
			// the first worker in builds the generator (the world model: a moment)
			std::lock_guard lock(m_mutex);
			if (!m_generator && m_error.empty()) {
				try {
					m_generator = std::make_unique<Generator>(m_horizon, m_atlas);
				} catch (const std::exception &e) {
					m_error = std::string("the world can't be built from the server's data: ") + e.what();
				}
			}
		}
		m_wake.notify_all();
		std::unique_lock lock(m_mutex);
		while (true) {
			m_wake.wait(lock, [this] { return m_stop || (!m_queue.empty() && m_generator); });
			if (m_stop)
				return;
			TileKey key = m_queue.front();
			m_queue.pop_front();
			m_busy.insert(id(key));
			lock.unlock();

			Result r{ key, {} };
			try {
				r.mesh = onGlobe(m_generator->build(key));
			} catch (const std::exception &e) {
				lock.lock();
				m_error = e.what();
				return;
			}

			lock.lock();
			m_results.push_back(std::move(r));
		}
	}

	// The tile's mesh as on a map, each vertex then put on the planet: a
	// tile's (x, y, z) at its place on the globe, round its middle; its
	// height over the sea (blocks) in the first texture coordinate, for
	// the haze.
	Mesh onGlobe(const Tile &tile) const
	{
		Mesh m = far::mesh(tile, m_colours);
		const TileKey &key = tile.key;
		const Globe::Vec mid = m_globe.middle(key);
		const float half = key.span() / 2.0f - 0.5f;
		// the tile's own axes at its middle, for the normals
		const auto axis = [&](double dx, double dy, double dz) {
			const Globe::Vec a = m_globe.point(key, half + dx, -0.5 + dy, half + dz),
					b = m_globe.point(key, half - dx, -0.5 - dy, half - dz);
			return v3f(a.x - b.x, a.y - b.y, a.z - b.z).normalize();
		};
		const v3f ex = axis(0.5, 0, 0), ey = axis(0, 0.5, 0), ez = axis(0, 0, 0.5);
		for (Mesh::Part &part : m.parts)
			for (video::S3DVertex &v : part.vertices) {
				const v3f t = v.Pos / BS;
				const Globe::Vec p = m_globe.point(key, t.X, t.Y, t.Z);
				v.Pos = v3f(p.x - mid.x, p.y - mid.y, p.z - mid.z) * BS;
				v.Normal = (ex * v.Normal.X + ey * v.Normal.Y + ez * v.Normal.Z).normalize();
				v.TCoords = v2f(t.Y + 0.5f - (m_horizon.water_level + 1), 0);
			}
		return m;
	}

	const planet::Atlas &m_atlas;
	const Horizon &m_horizon;
	const Colours &m_colours;
	const Globe &m_globe;
	std::unique_ptr<Generator> m_generator;

	std::mutex m_mutex;
	std::condition_variable m_wake;
	bool m_stop = false;
	std::deque<TileKey> m_queue;
	std::set<std::tuple<s32, s32, s32, s32>> m_busy; // being built, or built and not yet taken
	std::deque<Result> m_results;
	std::string m_error;
	std::vector<std::thread> m_threads; // last: started once everything above exists
};

Tiles::Tiles(const planet::Atlas &atlas, const Horizon &horizon, const Colours &colours) :
	m_atlas(atlas),
	m_horizon(horizon),
	m_globe(atlas.atlas().params().radius),
	m_workers(std::make_unique<Workers>(m_atlas, m_horizon, colours, m_globe))
{
}

Tiles::~Tiles() = default;

void Tiles::update(const Eye &eye)
{
	m_frame++;
	m_draw.clear();
	std::vector<std::pair<float, Key>> want;
	for (s32 face = 0; face < 6; face++)
		choose(Key(m_globe.top(), 0, 0, face), eye, m_draw, want);

	std::sort(want.begin(), want.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	std::vector<std::pair<float, TileKey>> keys;
	keys.reserve(want.size());
	for (const auto &w : want)
		keys.emplace_back(w.first, w.second.tile());
	m_workers->want(keys);

	install();
	if (m_frame % 60 == 0)
		evict();
}

std::vector<Tiles::Drawn> Tiles::drawn() const
{
	std::vector<Drawn> out;
	out.reserve(m_draw.size());
	for (const Key &key : m_draw)
		out.push_back({ key.tile(), m_globe.middle(key.tile()), m_tiles.at(key).parts });
	return out;
}

float Tiles::distance(const TileKey &key, const Eye &eye) const
{
	const Globe::Vec p = m_globe.middle(key);
	const double r = m_globe.radius(), sea = r + m_horizon.water_level + 1;
	const double d = std::hypot(p.x * sea / r - eye.at.x, p.y * sea / r - eye.at.y, p.z * sea / r - eye.at.z);
	return (float)std::max(d - m_globe.diagonal(key) / 2, 0.0);
}

bool Tiles::choose(const Key &key, const Eye &eye, std::vector<Key> &draw, std::vector<std::pair<float, Key>> &want)
{
	float d = distance(key.tile(), eye);
	if (d > eye.range)
		return true; // beyond the eye's reach: nothing to draw
	// high up, a tile's size on screen goes by how far it is through the air
	float seen = std::sqrt(d * d + eye.altitude * eye.altitude);
	bool split = key.level > 0 && seen - eye.cut < (float)key.tile().span() * eye.split;

	if (!split) {
		Entry &e = m_tiles[key];
		e.used = m_frame;
		if (e.ready) {
			draw.push_back(key);
			return true;
		}
		want.emplace_back(d, key);
		// meanwhile, finer tiles from before, if all of them are there
		size_t mark = draw.size();
		std::function<bool(const Key &)> finer = [&](const Key &k) {
			if (k.level == 0)
				return false;
			for (s32 c = 0; c < 4; c++) {
				const Key child = k.child(c);
				auto it = m_tiles.find(child);
				if (it != m_tiles.end() && it->second.ready) {
					it->second.used = m_frame;
					draw.push_back(child);
				} else if (!finer(child)) {
					return false;
				}
			}
			return true;
		};
		if (finer(key))
			return true;
		draw.resize(mark);
		return false;
	}

	size_t mark = draw.size();
	bool all = true;
	for (s32 c = 0; c < 4; c++)
		all &= choose(key.child(c), eye, draw, want);
	if (all)
		return true;
	// a finer tile isn't there yet: this one stands in for all four
	auto it = m_tiles.find(key);
	if (it != m_tiles.end() && it->second.ready) {
		draw.resize(mark);
		draw.push_back(key);
		it->second.used = m_frame;
		return true;
	}
	return false;
}

void Tiles::install()
{
	for (int n = 0; n < INSTALLS_PER_FRAME; n++) {
		std::optional<Workers::Result> result = m_workers->take();
		if (!result)
			return;
		Entry &e = m_tiles[Key(result->key)];
		e.parts.clear();
		for (Mesh::Part &part : result->mesh.parts) {
			auto buffer = make_irr<scene::SMeshBuffer>();
			buffer->Vertices->Data = std::move(part.vertices);
			buffer->Indices->Data = std::move(part.indices);
			buffer->recalculateBoundingBox();
			buffer->setHardwareMappingHint(scene::EHM_STATIC);
			e.parts.push_back(std::move(buffer));
		}
		e.ready = true;
	}
}

void Tiles::evict()
{
	for (auto it = m_tiles.begin(); it != m_tiles.end();) {
		if (m_frame - it->second.used > KEEP_FRAMES)
			it = m_tiles.erase(it);
		else
			++it;
	}
}

} // namespace far
