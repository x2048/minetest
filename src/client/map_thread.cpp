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

#include "client/map_thread.h"
#include "mapblock.h"
#include "map.h"
#include "gamedef.h"

#include <string>

void MapThread::queueReceivedBlock(v3s16 pos, std::string &data, Map *map, IGameDef *gamedef)
{
	queue_in.push_back({pos, data, map, gamedef});
	deferUpdate();
}

MapBlock *MapThread::getNextReceivedBlock()
{
	try {
		return queue_out.pop_front(0).map_block;
	}
	catch(ItemNotFoundException &) {
		return nullptr;
	}
}

void MapThread::doUpdate()
{
	try {
		while (true) {
			ReceiveBlockRequest request = queue_in.pop_front(-1);
			MapBlock *block = new MapBlock(request.map, request.pos, request.gamedef);

			std::istringstream istr(request.block_data, std::ios_base::binary);

			block->deSerialize(istr, server_ser_ver, false);
			block->deSerializeNetworkSpecific(istr);

			queue_out.push_back(ReceiveBlockResponse {block});
		}
	}
	catch (ItemNotFoundException &) {
		// no op
	}
}