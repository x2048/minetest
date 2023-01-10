/*
Minetest
Copyright (C) 2023 Dmitry Kostenko <codeforsmile@gmail.com>

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

#include "thread.h"
#include "container.h"
#include "map.h"
#include "gamedef.h"
#include "irrlichttypes.h"

#include <string>

struct ReceiveBlockRequest {
	v3s16 pos;
	std::string block_data;
};

struct ReceiveBlockResponse {
	MapBlock *map_block {nullptr};
}

/// @brief Background thread responsible for receiving and 
/// deserializing map blocks
class MapThread : UpdateThread {
public:
	MapThread(Map *map, IGameDef *gamedef) : UpdateThread("map-update"), map(map), gamedef(gamedef) {}

	void queueReceiveBlock(v3s16 pos, const std::string &data);
	/// @brief Returns the next block from the receive queue or nullptr if empty
	MapBlock *getNextReceivedBlock();

protected:
	void doUpdate() override;	
private:
	MutexedQueue<ReceiveBlockRequest> queue_in;
	MutexedQueue<ReceiveBlockResponse> queue_out;

	Map *map;
	IGameDef *gamedef;
};