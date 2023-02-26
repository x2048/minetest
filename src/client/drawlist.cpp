/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2023 x2048, Dmitry Kostenko <codeforsmile@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "client/drawlist.h"
#include "client/client.h"
#include "client/camera.h"
#include "client/clientmap.h"
#include "mapblock.h"
#include "mapsector.h"
#include "nodedef.h"
#include "profiler.h"
#include "util/numeric.h"

#include <map>
#include <vector>

DrawList::DrawList(ClientMap *map) : m_map(map), m_drawlist(MapBlockComparer(v3s16(0, 0, 0)))
{
}

DrawList::~DrawList()
{
	for (const auto p : m_drawlist) {
		auto block = p.second;
		if (block)
			block->refDrop();
	}

	for (const auto block : m_keeplist)
		if (block)
			block->refDrop();
}

class MapBlockFlags
{
public:
	static constexpr u16 CHUNK_EDGE = 8;
	static constexpr u16 CHUNK_MASK = CHUNK_EDGE - 1;
	static constexpr std::size_t CHUNK_VOLUME = CHUNK_EDGE * CHUNK_EDGE * CHUNK_EDGE; // volume of a chunk

	MapBlockFlags(v3s16 min_pos, v3s16 max_pos)
			: min_pos(min_pos), volume((max_pos - min_pos) / CHUNK_EDGE + 1)
	{
		chunks.resize(volume.X * volume.Y * volume.Z);
	}

	class Chunk
	{
	public:
		inline u8 &getBits(v3s16 pos)
		{
			std::size_t address = getAddress(pos);
			return bits[address];
		}

	private:
		inline std::size_t getAddress(v3s16 pos) {
			std::size_t address = (pos.X & CHUNK_MASK) + (pos.Y & CHUNK_MASK) * CHUNK_EDGE + (pos.Z & CHUNK_MASK) * (CHUNK_EDGE * CHUNK_EDGE);
			return address;
		}

		std::array<u8, CHUNK_VOLUME> bits;
	};

	Chunk &getChunk(v3s16 pos)
	{
		v3s16 delta = (pos - min_pos) / CHUNK_EDGE;
		std::size_t address = delta.X + delta.Y * volume.X + delta.Z * volume.X * volume.Y;
		Chunk *chunk = chunks[address].get();
		if (!chunk) {
			chunk = new Chunk();
			chunks[address].reset(chunk);
		}
		return *chunk;
	}
private:
	std::vector<std::unique_ptr<Chunk>> chunks;
	v3s16 min_pos;
	v3s16 volume;
};

void DrawList::update(v3f m_camera_position, const MapDrawControl &m_control, const NodeDefManager *m_nodedef, Client *m_client, bool m_enable_raytraced_culling)
{
	ScopeProfiler sp(g_profiler, "CM::updateDrawList()", SPT_AVG);

	m_needs_update_drawlist = false;

	for (auto &i : m_drawlist) {
		MapBlock *block = i.second;
		block->refDrop();
	}
	m_drawlist.clear();

	for (auto &block : m_keeplist) {
		block->refDrop();
	}
	m_keeplist.clear();

	v3s16 cam_pos_nodes = floatToInt(m_camera_position, BS);

	v3s16 p_blocks_min;
	v3s16 p_blocks_max;
	getBlocksInViewRange(cam_pos_nodes, &p_blocks_min, &p_blocks_max, m_control.range_all ? m_control.loaded_range : -1.0f);

	// Number of blocks occlusion culled
	u32 blocks_occlusion_culled = 0;
	// Number of blocks frustum culled
	u32 blocks_frustum_culled = 0;
	// Blocks visited by the algorithm
	u32 blocks_visited = 0;
	// Block sides that were not traversed
	u32 sides_skipped = 0;

	MeshGrid mesh_grid = m_client->getMeshGrid();

	// No occlusion culling when free_move is on and camera is inside ground
	// No occlusion culling for chunk sizes of 4 and above
	//   because the current occlusion culling test is highly inefficient at these sizes
	bool occlusion_culling_enabled = mesh_grid.cell_size < 4;
	if (m_control.allow_noclip) {
		MapNode n = getNode(cam_pos_nodes);
		if (n.getContent() == CONTENT_IGNORE || m_nodedef->get(n).solidness == 2)
			occlusion_culling_enabled = false;
	}

	v3s16 camera_block = getContainerPos(cam_pos_nodes, MAP_BLOCKSIZE);
	m_drawlist = std::map<v3s16, MapBlock*, MapBlockComparer>(MapBlockComparer(camera_block));

	auto is_frustum_culled = m_client->getCamera()->getFrustumCuller();

	// Uncomment to debug occluded blocks in the wireframe mode
	// TODO: Include this as a flag for an extended debugging setting
	// if (occlusion_culling_enabled && m_control.show_wireframe)
	// 	occlusion_culling_enabled = porting::getTimeS() & 1;

	std::queue<v3s16> blocks_to_consider;

	v3s16 camera_mesh = mesh_grid.getMeshPos(camera_block);
	v3s16 camera_cell = mesh_grid.getCellPos(camera_block);

	// Bits per block:
	// [ visited | 0 | 0 | 0 | 0 | Z visible | Y visible | X visible ]
	MapBlockFlags meshes_seen(mesh_grid.getCellPos(p_blocks_min), mesh_grid.getCellPos(p_blocks_max) + 1);

	// Start breadth-first search with the block the camera is in
	blocks_to_consider.push(camera_mesh);
	meshes_seen.getChunk(camera_cell).getBits(camera_cell) = 0x07; // mark all sides as visible

	std::set<v3s16> shortlist;

	// Recursively walk the space and pick mapblocks for drawing
	while (blocks_to_consider.size() > 0) {

		v3s16 block_coord = blocks_to_consider.front();
		blocks_to_consider.pop();

		v3s16 cell_coord = mesh_grid.getCellPos(block_coord);
		auto &flags = meshes_seen.getChunk(cell_coord).getBits(cell_coord);

		// Only visit each block once (it may have been queued up to three times)
		if ((flags & 0x80) == 0x80)
			continue;
		flags |= 0x80;

		blocks_visited++;

		// Get the sector, block and mesh
		MapSector *sector = this->getSectorNoGenerate(v2s16(block_coord.X, block_coord.Z));

		MapBlock *block = sector ? sector->getBlockNoCreateNoEx(block_coord.Y) : nullptr;

		MapBlockMesh *mesh = block ? block->mesh : nullptr;

		// Calculate the coordinates for range and frustum culling
		v3f mesh_sphere_center;
		f32 mesh_sphere_radius;

		v3s16 block_pos_nodes = block_coord * MAP_BLOCKSIZE;

		if (mesh) {
			mesh_sphere_center = intToFloat(block_pos_nodes, BS)
					+ mesh->getBoundingSphereCenter();
			mesh_sphere_radius = mesh->getBoundingRadius();
		}
		else {
			mesh_sphere_center = intToFloat(block_pos_nodes, BS) + v3f((mesh_grid.cell_size * MAP_BLOCKSIZE * 0.5f - 0.5f) * BS);
			mesh_sphere_radius = 0.87f * mesh_grid.cell_size * MAP_BLOCKSIZE * BS;
		}

		// First, perform a simple distance check.
		if (!m_control.range_all &&
			mesh_sphere_center.getDistanceFrom(intToFloat(cam_pos_nodes, BS)) >
				m_control.wanted_range * BS + mesh_sphere_radius)
			continue; // Out of range, skip.

		// Frustum culling
		// Only do coarse culling here, to account for fast camera movement.
		// This is needed because this function is not called every frame.
		float frustum_cull_extra_radius = 300.0f;
		if (is_frustum_culled(mesh_sphere_center,
				mesh_sphere_radius + frustum_cull_extra_radius)) {
			blocks_frustum_culled++;
			continue;
		}

		// Calculate the vector from the camera block to the current block
		// We use it to determine through which sides of the current block we can continue the search
		v3s16 look = block_coord - camera_mesh;

		// Occluded near sides will further occlude the far sides
		u8 visible_outer_sides = flags & 0x07;

		// Raytraced occlusion culling - send rays from the camera to the block's corners
		if (occlusion_culling_enabled && m_enable_raytraced_culling &&
				block && mesh &&
				visible_outer_sides != 0x07 && m_map->isMeshOccluded(block, mesh_grid.cell_size, cam_pos_nodes)) {
			blocks_occlusion_culled++;
			continue;
		}

		if (mesh_grid.cell_size > 1) {
			// Block meshes are stored in the corner block of a chunk
			// (where all coordinate are divisible by the chunk size)
			// Add them to the de-dup set.
			shortlist.emplace(block_coord.X, block_coord.Y, block_coord.Z);
			// All other blocks we can grab and add to the keeplist right away.
			if (block) {
				m_keeplist.push_back(block);
				block->refGrab();
			}
		}
		else if (mesh) {
			// without mesh chunking we can add the block to the drawlist
			block->refGrab();
			m_drawlist.emplace(block_coord, block);
		}

		// Decide which sides to traverse next or to block away

		// First, find the near sides that would occlude the far sides
		// * A near side can itself be occluded by a nearby block (the test above ^^)
		// * A near side can be visible but fully opaque by itself (e.g. ground at the 0 level)

		// mesh solid sides are +Z-Z+Y-Y+X-X
		// if we are inside the block's coordinates on an axis, 
		// treat these sides as opaque, as they should not allow to reach the far sides
		u8 block_inner_sides = (look.X == 0 ? 3 : 0) |
			(look.Y == 0 ? 12 : 0) |
			(look.Z == 0 ? 48 : 0);

		// get the mask for the sides that are relevant based on the direction
		u8 near_inner_sides = (look.X > 0 ? 1 : 2) |
				(look.Y > 0 ? 4 : 8) |
				(look.Z > 0 ? 16 : 32);
		
		// This bitset is +Z-Z+Y-Y+X-X (See MapBlockMesh), and axis is XYZ.
		// Get he block's transparent sides
		u8 transparent_sides = (occlusion_culling_enabled && block) ? ~block->solid_sides : 0x3F;

		// compress block transparent sides to ZYX mask of see-through axes
		u8 near_transparency =  (block_inner_sides == 0x3F) ? near_inner_sides : (transparent_sides & near_inner_sides);

		// when we are inside the camera block, do not block any sides
		if (block_inner_sides == 0x3F)
			block_inner_sides = 0;

		near_transparency &= ~block_inner_sides & 0x3F;

		near_transparency |= (near_transparency >> 1);
		near_transparency = (near_transparency & 1) |
				((near_transparency >> 1) & 2) |
				((near_transparency >> 2) & 4);

		// combine with known visible sides that matter
		near_transparency &= visible_outer_sides;

		// The rule for any far side to be visible:
		// * Any of the adjacent near sides is transparent (different axes)
		// * The opposite near side (same axis) is transparent, if it is the dominant axis of the look vector

		// Calculate vector from camera to mapblock center. Because we only need relation between
		// coordinates we scale by 2 to avoid precision loss.
		v3s16 precise_look = 2 * (block_pos_nodes - cam_pos_nodes) + mesh_grid.cell_size * MAP_BLOCKSIZE - 1;

		// dominant axis flag
		u8 dominant_axis = (abs(precise_look.X) > abs(precise_look.Y) && abs(precise_look.X) > abs(precise_look.Z)) |
					((abs(precise_look.Y) > abs(precise_look.Z) && abs(precise_look.Y) > abs(precise_look.X)) << 1) |
					((abs(precise_look.Z) > abs(precise_look.X) && abs(precise_look.Z) > abs(precise_look.Y)) << 2);

		// Queue next blocks for processing:
		// - Examine "far" sides of the current blocks, i.e. never move towards the camera
		// - Only traverse the sides that are not occluded
		// - Only traverse the sides that are not opaque
		// When queueing, mark the relevant side on the next block as 'visible'
		for (s16 axis = 0; axis < 3; axis++) {

			// Select a bit from transparent_sides for the side
			u8 far_side_mask = 1 << (2 * axis);

			// axis flag
			u8 my_side = 1 << axis;
			u8 adjacent_sides = my_side ^ 0x07;

			auto traverse_far_side = [&](s8 next_pos_offset) {
				// far side is visible if adjacent near sides are transparent, or if opposite side on dominant axis is transparent
				bool side_visible = ((near_transparency & adjacent_sides) | (near_transparency & my_side & dominant_axis)) != 0;
				side_visible = side_visible && ((far_side_mask & transparent_sides) != 0);

				v3s16 next_pos = block_coord;
				next_pos[axis] += next_pos_offset;

				v3s16 next_cell = mesh_grid.getCellPos(next_pos);

				// If a side is a see-through, mark the next block's side as visible, and queue
				if (side_visible) {
					auto &next_flags = meshes_seen.getChunk(next_cell).getBits(next_cell);
					next_flags |= my_side;
					blocks_to_consider.push(next_pos);
				}
				else {
					sides_skipped++;
				}
			};


			// Test the '-' direction of the axis
			if (look[axis] <= 0 && block_coord[axis] > p_blocks_min[axis])
				traverse_far_side(-mesh_grid.cell_size);

			// Test the '+' direction of the axis
			far_side_mask <<= 1;

			if (look[axis] >= 0 && block_coord[axis] < p_blocks_max[axis])
				traverse_far_side(+mesh_grid.cell_size);
		}
	}

	g_profiler->avg("MapBlocks shortlist [#]", shortlist.size());

	assert(m_drawlist.empty() || shortlist.empty());
	for (auto pos : shortlist) {
		MapBlock * block = getBlockNoCreateNoEx(pos);
		if (block) {
			block->refGrab();
			m_drawlist.emplace(pos, block);
		}
	}

	g_profiler->avg("MapBlocks occlusion culled [#]", blocks_occlusion_culled);
	g_profiler->avg("MapBlocks frustum culled [#]", blocks_frustum_culled);
	g_profiler->avg("MapBlocks sides skipped [#]", sides_skipped);
	g_profiler->avg("MapBlocks examined [#]", blocks_visited);
	g_profiler->avg("MapBlocks drawn [#]", m_drawlist.size());
}

MapNode DrawList::getNode(v3s16 p, bool *is_valid_position)
{
	return m_map->getNode(p, is_valid_position);
}

MapBlock *DrawList::getBlockNoCreateNoEx(v3s16 pos)
{
	return m_map->getBlockNoCreateNoEx(pos);
}

MapSector *DrawList::getSectorNoGenerate(v2s16 pos)
{
	return m_map->getSectorNoGenerate(pos);
}

bool DrawList::isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes)
{
	return m_map->isBlockOccluded(block, cam_pos_nodes);
}

void DrawList::getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range)
{
	m_map->getBlocksInViewRange(cam_pos_nodes, p_blocks_min, p_blocks_max, range);
}
