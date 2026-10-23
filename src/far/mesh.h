// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "far/terrain.h"
#include "irrlichttypes_bloated.h"
#include <S3DVertex.h>
#include <unordered_map>
#include <vector>

class ITextureSource;
class NodeDefManager;

namespace far {

// A block's faces, in tile order: up, down, +X, −X, +Z, −Z.
enum Face { UP, DOWN, EAST, WEST, NORTH, SOUTH };

/*
	What each block looks like from afar: the average of each face's texture
	(what the GPU's smallest mipmap shows), tinted as the near mesher tints it
	(palette by param2, or the tile's own colour). Built on the main thread
	(textures live there), then read-only: the mesh workers share it.
*/
class Colours
{
public:
	Colours(const NodeDefManager *ndef, ITextureSource *tsrc, const std::vector<content_t> &blocks);

	// 0..1 sRGB
	video::SColorf face(MapNode n, Face f) const;
	// A liquid's top seen over `floor` lit to `lit` (0..1): its texture laid
	// over what's below it, pixel by pixel, as the near world blends it.
	video::SColorf through(MapNode liquid, video::SColorf floor, float lit) const;

private:
	struct Entry
	{
		video::SColorf face[6];
		bool tile_colour[6];
		video::SColorf over_black; // the top texture over black: its colour times its opacity
		float clarity;             // how much shows through it: 1 − its opacity
	};
	const NodeDefManager *m_ndef;
	std::unordered_map<content_t, Entry> m_entries;
};

// The blocks a Horizon can put in view.
std::vector<content_t> blocksIn(const Horizon &h);

// A tile as triangles: in blocks relative to the tile's origin, scaled by
// BS; vertex colours carry each face's colour and shade (as the near mesh's
// do in daylight). Split into parts that fit 16-bit indices.
struct Mesh
{
	struct Part
	{
		std::vector<video::S3DVertex> vertices;
		std::vector<u16> indices;
	};
	std::vector<Part> parts;
};

Mesh mesh(const Tile &tile, const Colours &colours);

} // namespace far
