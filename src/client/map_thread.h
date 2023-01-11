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

#include "util/thread.h"
#include "util/container.h"
#include "irrlichttypes.h"

#include <string>

class Map;
class MapBlock;
class IGameDef;

struct ReceiveBlockRequest {
	v3s16 pos;
	std::string block_data;
	Map *map;
	IGameDef *gamedef;
};

struct ReceiveBlockResponse {
	MapBlock *map_block {nullptr};
};

/// @brief Background thread responsible for receiving and 
/// deserializing map blocks
class MapThread : public UpdateThread {
public:
	MapThread() : UpdateThread("map-update") {}

	void queueReceivedBlock(v3s16 pos, std::string &data, Map *map, IGameDef *gamedef);
	/// @brief Returns the next block from the receive queue or nullptr if empty
	MapBlock *getNextReceivedBlock();

	u8 server_ser_ver;

protected:
	void doUpdate() override;	
private:
	MutexedQueue<ReceiveBlockRequest> queue_in;
	MutexedQueue<ReceiveBlockResponse> queue_out;
};