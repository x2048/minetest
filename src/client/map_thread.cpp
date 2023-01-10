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

#include <stringstream>

void MapThread::queueReceiveBlock(v3s16 pos, std::string &data)
{
	queue_in.push_back({pos, std::move(data)});
}

MapBlock *MapThread::getNextReceivedBlock()
{
	try {
		return queue_out.pop_front(0).map_block;
	}
	catch(ItemNotFoundException) {
		return nullptr;
	}
}

void MapThread::doUpdate()
{
	try {
		while (true) {
			ReceiveBlockRequest request = queue_in.pop_front();
			MapBlock *block = new MapBlock(map, request.pos, gamedef);

			std::stringstream
			block->deSerialize(request)
		}
	}
	catch (ItemNotFoundException)
	{
		// no op
	}
}