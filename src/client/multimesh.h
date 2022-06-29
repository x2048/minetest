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

#pragma once

#include "irrlichttypes_extrabloated.h"

#include <stack>

/**
 * Represents an allocated range in memory or a buffer
 */
struct Range
{
	u32 start;
	u32 length;

    bool operator < (const Range & other) const {
        return start < other.start;
    }
};

/**
 * Allocates ranges in a continuous buffer.
 * 
 * This is useful to manage multiple meshes in a single buffer such as mapblock or particle meshes
 */
class Allocator
{
public:
	Allocator(u32 max_size) : max_size(max_size) {}

	bool allocate(u32 size, u32 *id)
	{
		Range *best_match { nullptr };
		for (auto &extent : free)
			if (extent.length > size && (!best_match || extent.length - size < best_match->length - size))
				best_match = &extent;
		
		Range result { 0, size };
		if (best_match) {
			result.start = best_match->start;
			best_match->start += size;
			best_match->length -= size;
		}
		else if (current_size + size > max_size) {
            *id = 0;
            return false;
        }
        else {
			result.start = current_size;
			current_size += size;
		}

		if (free_ids.empty()) {
			*id = allocated.size();
			allocated.push_back(result);
		}
		else {
			*id = free_ids.top();
			free_ids.pop();
		}

        allocated[*id] = result;

		return true;
	}

	Range getRange(u32 id)
	{
		return allocated[id];
	}

	void release(u32 id)
	{
        Range extent = allocated[id];
        if (extent.start + extent.length == current_size) {
            // merge the last allocated extent immediately
            current_size -= extent.length;
        }
        else {
		    free.push_back(extent);
        }
		free_ids.push(id);
	}

	void compact(void (*move) (Range from, Range to))
	{
	}
private:
	u32 max_size;
	u32 current_size {0};
	std::vector<Range> allocated;
	std::vector<Range> free;
	std::stack<u32> free_ids;
};


class SharedMeshBuffer : public scene::SMeshBuffer
{
public:
    SharedMeshBuffer() : vertex_allocator(65536), index_allocator(65536) {}

	virtual void append(const void* const vertices, u32 numVertices, const u16* const indices, u32 numIndices) _IRR_OVERRIDE_ {
		assert(false);
	}

    bool append(const void* const vertices, u32 numVertices, const u16* const indices, u32 numIndices, u32 *vertex_id, u32 *index_id) {
        if (!vertex_allocator.allocate(numVertices, vertex_id))
            return false;
        if (!index_allocator.allocate(numIndices, index_id)) {
            vertex_allocator.release(*vertex_id);
            return false;
        }

        Range vertex_range = vertex_allocator.getRange(*vertex_id);
        Vertices.set_used(vertex_range.start + vertex_range.length);
		std::move((video::S3DVertex*)vertices, (video::S3DVertex*)vertices + vertex_range.length, &Vertices[vertex_range.start]);

        Range index_range = index_allocator.getRange(*index_id);
        Indices.set_used(index_range.start + index_range.length);
		std::move(indices, indices + index_range.length, &Indices[index_range.start]);

        return true;
    }

    Range getVertexRange(u16 id)
    {
        return vertex_allocator.getRange(id);
    }

    Range getIndexRange(u16 id)
    {
        return index_allocator.getRange(id);
    }
    
	virtual void release(u16 vertex_id, u16 index_id)
	{
		vertex_allocator.release(vertex_id);
        index_allocator.release(index_id);
	}

private:
	std::vector<video::S3DVertex> vertices;
	Allocator vertex_allocator;
    Allocator index_allocator;
};

class DynamicMeshBuffer
{
public:
	DynamicMeshBuffer(const video::SMaterial &material) : material(material) {}
private:
	std::vector<SharedMeshBuffer> buffers;
};

class EmbeddedMeshBuffer : scene::IMeshBuffer
{
public:
    EmbeddedMeshBuffer(SharedMeshBuffer *main_buffer, u16 vertex_id, u16 index_id) {}
	//! Get the material of this meshbuffer
	/** \return Material of this buffer. */
	virtual video::SMaterial& getMaterial() override {

    }

	//! Get the material of this meshbuffer
	/** \return Material of this buffer. */
	virtual const video::SMaterial& getMaterial() const override;

	//! Get type of vertex data which is stored in this meshbuffer.
	/** \return Vertex type of this buffer. */
	virtual video::E_VERTEX_TYPE getVertexType() const override;

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	virtual const void* getVertices() const override;

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	virtual void* getVertices() override;

	//! Get amount of vertices in meshbuffer.
	/** \return Number of vertices in this buffer. */
	virtual u32 getVertexCount() const override;

	//! Get type of index data which is stored in this meshbuffer.
	/** \return Index type of this buffer. */
	virtual video::E_INDEX_TYPE getIndexType() const =0;

	//! Get access to indices.
	/** \return Pointer to indices array. */
	virtual const u16* getIndices() const override;

	//! Get access to indices.
	/** \return Pointer to indices array. */
	virtual u16* getIndices() override;

	//! Get amount of indices in this meshbuffer.
	/** \return Number of indices in this buffer. */
	virtual u32 getIndexCount() const override;

	//! Get the axis aligned bounding box of this meshbuffer.
	/** \return Axis aligned bounding box of this buffer. */
	virtual const core::aabbox3df& getBoundingBox() const override;

	//! Set axis aligned bounding box
	/** \param box User defined axis aligned bounding box to use
	for this buffer. */
	virtual void setBoundingBox(const core::aabbox3df& box) override;

	//! Recalculates the bounding box. Should be called if the mesh changed.
	virtual void recalculateBoundingBox() override;

	//! returns position of vertex i
	virtual const core::vector3df& getPosition(u32 i) const override;

	//! returns position of vertex i
	virtual core::vector3df& getPosition(u32 i) override;

	//! returns normal of vertex i
	virtual const core::vector3df& getNormal(u32 i) const override;

	//! returns normal of vertex i
	virtual core::vector3df& getNormal(u32 i) override;

	//! returns texture coord of vertex i
	virtual const core::vector2df& getTCoords(u32 i) const override;

	//! returns texture coord of vertex i
	virtual core::vector2df& getTCoords(u32 i) override
	{
		assert(false);
	}

	//! Append the vertices and indices to the current buffer
	/** Only works for compatible vertex types.
	\param vertices Pointer to a vertex array.
	\param numVertices Number of vertices in the array.
	\param indices Pointer to index array.
	\param numIndices Number of indices in array. */
	virtual void append(const void* const vertices, u32 numVertices, const u16* const indices, u32 numIndices) override
	{

	}

	//! Append the meshbuffer to the current buffer
	/** Only works for compatible vertex types
	\param other Buffer to append to this one. */
	virtual void append(const scene::IMeshBuffer* const other) override
	{
		assert(false);
	}

	//! get the current hardware mapping hint
	virtual scene::E_HARDWARE_MAPPING getHardwareMappingHint_Vertex() const override
	{
		return inner_buffer->getHardwareMappingHint_Index();
	}

	//! get the current hardware mapping hint
	virtual scene::E_HARDWARE_MAPPING getHardwareMappingHint_Index() const override
	{
		return inner_buffer->getHardwareMappingHint_Index();
	}

	//! set the hardware mapping hint, for driver
	virtual void setHardwareMappingHint(scene::E_HARDWARE_MAPPING newMappingHint, scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX ) override
	{
		inner_buffer->setHardwareMappingHint(newMappingHint, buffer);
	}

	//! flags the meshbuffer as changed, reloads hardware buffers
	virtual void setDirty(scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX) override
	{
		inner_buffer->setDirty(buffer);
	}

	//! Get the currently used ID for identification of changes.
	/** This shouldn't be used for anything outside the VideoDriver. */
	virtual u32 getChangedID_Vertex() const override
	{
		return inner_buffer->getChangedID_Index();
	}

	//! Get the currently used ID for identification of changes.
	/** This shouldn't be used for anything outside the VideoDriver. */
	virtual u32 getChangedID_Index() const override
	{
		return inner_buffer->getChangedID_Index();
	}

	//! Used by the VideoDriver to remember the buffer link.
	virtual void setHWBuffer(void *ptr) const override
	{
		inner_buffer->setHWBuffer(ptr);
	}

	virtual void *getHWBuffer() const override
	{
		return inner_buffer->getHWBuffer();
	}

	//! Describe what kind of primitive geometry is used by the meshbuffer
	/** Note: Default is EPT_TRIANGLES. Using other types is fine for rendering.
	But meshbuffer manipulation functions might expect type EPT_TRIANGLES
	to work correctly. Also mesh writers will generally fail (badly!) with other
	types than EPT_TRIANGLES. */
	virtual void setPrimitiveType(scene::E_PRIMITIVE_TYPE type) override
	{
		assert(false);
	}

	//! Get the kind of primitive geometry which is used by the meshbuffer
	virtual scene::E_PRIMITIVE_TYPE getPrimitiveType() const override
	{
		return inner_buffer->getPrimitiveType();
	}

	//! Calculate how many geometric primitives are used by this meshbuffer
	virtual u32 getPrimitiveCount() const override
	{
		
		const u32 indexCount = getIndexCount();
		switch (getPrimitiveType())
		{
			case scene::EPT_POINTS:	        return indexCount;
			case scene::EPT_LINE_STRIP:     return indexCount-1;
			case scene::EPT_LINE_LOOP:      return indexCount;
			case scene::EPT_LINES:          return indexCount/2;
			case scene::EPT_TRIANGLE_STRIP: return (indexCount-2);
			case scene::EPT_TRIANGLE_FAN:   return (indexCount-2);
			case scene::EPT_TRIANGLES:      return indexCount/3;
			case scene::EPT_QUAD_STRIP:     return (indexCount-2)/2;
			case scene::EPT_QUADS:          return indexCount/4;
			case scene::EPT_POLYGON:        return indexCount; // (not really primitives, that would be 1, works like line_strip)
			case scene::EPT_POINT_SPRITES:  return indexCount;
		}
		return 0;
	}

private:
	scene::IMeshBuffer *inner_buffer;
};