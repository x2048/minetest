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

#pragma once

#include "mapnode.h"
#include "irrlichttypes_bloated.h"
#include <map>
#include <vector>

class ClientMap;
class Camera;
class MapBlock;
class MapSector;
class Client;
class NodeDefManager;
class MapDrawControl;

// Orders blocks by distance to the camera
class MapBlockComparer
{
public:
	MapBlockComparer(const v3s16 &camera_block) : m_camera_block(camera_block) {}

	bool operator() (const v3s16 &left, const v3s16 &right) const
	{
		auto distance_left = left.getDistanceFromSQ(m_camera_block);
		auto distance_right = right.getDistanceFromSQ(m_camera_block);
		return distance_left > distance_right || (distance_left == distance_right && left > right);
	}

private:
	v3s16 m_camera_block;
};

class DrawList
{
public:
	DrawList(ClientMap *map);
	~DrawList();

	void update(v3f m_camera_position, const MapDrawControl &m_control, const NodeDefManager *m_nodedef, Client *m_client, bool m_enable_raytraced_culling);
	const std::map<v3s16, MapBlock *, MapBlockComparer> &getDrawList() const { return m_drawlist; }

	bool needsUpdateDrawList() const { return m_needs_update_drawlist; }
	void setNeedsUpdateDrawList(bool value) { m_needs_update_drawlist = value; }
private:
	ClientMap *m_map;
	std::map<v3s16, MapBlock*, MapBlockComparer> m_drawlist;
	std::vector<MapBlock*> m_keeplist;
	bool m_needs_update_drawlist;

	MapNode getNode(v3s16 p, bool *is_valid_position = NULL);
	MapBlock *getBlockNoCreateNoEx(v3s16 pos);
	MapSector *getSectorNoGenerate(v2s16 pos);
	bool isBlockOccluded(MapBlock *block, v3s16 cam_pos_nodes);
	void getBlocksInViewRange(v3s16 cam_pos_nodes,
			v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range=-1.0f);
};

