/*
Minetest
Copyright (C) 2022 x2048, Dmitry Kostenko <codeforsmile@gmail.com>

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

#include "test.h"

#include "log.h"
#include "client/multimesh.h"

class TestAllocator : public TestBase
{
public:
	TestAllocator() { TestManager::registerTestModule(this); }

	const char *getName() { return "TestAllocator"; }

	void runTests(IGameDef *gamedef);

	void testAllocateReturnsRequestedSize();
	void testCannotAllocateOverMaxSize();
	void testCanAllocateAfterRelease();
};

static TestAllocator g_test_instance;

void TestAllocator::runTests(IGameDef *gamedef)
{
	TEST(testAllocateReturnsRequestedSize);
	TEST(testCannotAllocateOverMaxSize);
	TEST(testCanAllocateAfterRelease);
}

void TestAllocator::testAllocateReturnsRequestedSize()
{
    Allocator alloc(10);
    u32 id;
    UASSERT(alloc.allocate(5, &id));
	Range range = alloc.getRange(id);
    UASSERT(range.length == 5);
}

void TestAllocator::testCannotAllocateOverMaxSize()
{
    Allocator alloc(10);
    u32 id;

    UASSERT(alloc.allocate(5, &id));
	UASSERT(alloc.allocate(5, &id));
	UASSERT(!alloc.allocate(1, &id));
}

void TestAllocator::testCanAllocateAfterRelease()
{
    Allocator alloc(10);
    u32 id;

    UASSERT(alloc.allocate(5, &id));
	UASSERT(alloc.allocate(5, &id));
	alloc.release(id);
	UASSERT(alloc.allocate(1, &id));
}
