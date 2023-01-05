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
#include <vector>

/**
 * Implements an IMeshBuffer which is a projection into another IMeshBuffer
 * 
 */
class ProxyMeshBuffer : public scene::IMeshBuffer
{
public:
    ProxyMeshBuffer(scene::IMeshBuffer *_origin, u16 _vertex_base, u16 _vertex_count, u16 _index_base, u16 _index_count)
        : origin(_origin), vertex_base(_vertex_base), vertex_count(_vertex_count), index_base(_index_base), index_count(_index_count)
    {
        origin->grab();
    }

    virtual ~ProxyMeshBuffer()
    {
        origin->drop();
    }

    //! Get the material of this meshbuffer
    /** \return Material of this buffer. */
    virtual video::SMaterial& getMaterial()
    {
        return origin->getMaterial();
    }

    //! Get the material of this meshbuffer
    /** \return Material of this buffer. */
    virtual const video::SMaterial& getMaterial() const
    {
        return origin->getMaterial();
    }

    //! Get type of vertex data which is stored in this meshbuffer.
    /** \return Vertex type of this buffer. */
    virtual video::E_VERTEX_TYPE getVertexType() const
    {
        return origin->getVertexType();
    }

    //! Get access to vertex data. The data is an array of vertices.
    /** Which vertex type is used can be determined by getVertexType().
    \return Pointer to array of vertices. */
    virtual const void* getVertices() const
    {
        return (video::S3DVertex*)origin->getVertices() + vertex_base;
    }

    //! Get access to vertex data. The data is an array of vertices.
    /** Which vertex type is used can be determined by getVertexType().
    \return Pointer to array of vertices. */
    virtual void* getVertices()
    {
        return (video::S3DVertex*)origin->getVertices() + vertex_base;
    }

    //! Get amount of vertices in meshbuffer.
    /** \return Number of vertices in this buffer. */
    virtual u32 getVertexCount() const
    {
        return vertex_count;
    }

    //! Get type of index data which is stored in this meshbuffer.
    /** \return Index type of this buffer. */
    virtual video::E_INDEX_TYPE getIndexType() const
    {
        return origin->getIndexType();
    }

    //! Get access to indices.
    /** \return Pointer to indices array. */
    virtual const u16* getIndices() const
    {
        return origin->getIndices() + index_base;
    }

    //! Get access to indices.
    /** \return Pointer to indices array. */
    virtual u16* getIndices()
    {
        return origin->getIndices() + index_base;
    }

    //! Get amount of indices in this meshbuffer.
    /** \return Number of indices in this buffer. */
    virtual u32 getIndexCount() const
    {
        return index_count;
    }

    //! Get the axis aligned bounding box of this meshbuffer.
    /** \return Axis aligned bounding box of this buffer. */
    virtual const core::aabbox3df& getBoundingBox() const
    {
        return bounding_box;
    }

    //! Set axis aligned bounding box
    /** \param box User defined axis aligned bounding box to use
    for this buffer. */
    virtual void setBoundingBox(const core::aabbox3df& box)
    {
        bounding_box = box;
    }

    //! Recalculates the bounding box. Should be called if the mesh changed.
    virtual void recalculateBoundingBox();

    //! returns position of vertex i
    virtual const core::vector3df& getPosition(u32 i) const
    {
        return origin->getPosition(i + vertex_base);
    }

    //! returns position of vertex i
    virtual core::vector3df& getPosition(u32 i)
    {
        return origin->getPosition(i + vertex_base);
    }

    //! returns normal of vertex i
    virtual const core::vector3df& getNormal(u32 i) const
    {
        return origin->getNormal(i + vertex_base);
    }

    //! returns normal of vertex i
    virtual core::vector3df& getNormal(u32 i)
    {
        return origin->getNormal(i + vertex_base);
    }

    //! returns texture coord of vertex i
    virtual const core::vector2df& getTCoords(u32 i) const
    {
        return origin->getTCoords(i + vertex_base);
    }

    //! returns texture coord of vertex i
    virtual core::vector2df& getTCoords(u32 i)
    {
        return origin->getTCoords(i + vertex_base);
    }

    //! Append the vertices and indices to the current buffer
    /** Only works for compatible vertex types.
    \param vertices Pointer to a vertex array.
    \param numVertices Number of vertices in the array.
    \param indices Pointer to index array.
    \param numIndices Number of indices in array. */
    virtual void append(const void* const vertices, u32 numVertices, const u16* const indices, u32 numIndices)
    {
        // this method is not implemented
        assert(false);
    }

    //! Append the meshbuffer to the current buffer
    /** Only works for compatible vertex types
    \param other Buffer to append to this one. */
    virtual void append(const IMeshBuffer* const other)
    {
        // this method is not implemented
        assert(false);
    }

    //! get the current hardware mapping hint
    virtual scene::E_HARDWARE_MAPPING getHardwareMappingHint_Vertex() const
    {
        return origin->getHardwareMappingHint_Vertex();
    }

    //! get the current hardware mapping hint
    virtual scene::E_HARDWARE_MAPPING getHardwareMappingHint_Index() const
    {
        return origin->getHardwareMappingHint_Index();
    }

    //! set the hardware mapping hint, for driver
    virtual void setHardwareMappingHint( scene::E_HARDWARE_MAPPING newMappingHint, scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX )
    {
        origin->setHardwareMappingHint(newMappingHint, buffer);
    }

    //! flags the meshbuffer as changed, reloads hardware buffers
    virtual void setDirty(scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX)
    {
        origin->setDirty(buffer);
    }

    //! Get the currently used ID for identification of changes.
    /** This shouldn't be used for anything outside the VideoDriver. */
    virtual u32 getChangedID_Vertex() const
    {
        return origin->getChangedID_Vertex();
    }

    //! Get the currently used ID for identification of changes.
    /** This shouldn't be used for anything outside the VideoDriver. */
    virtual u32 getChangedID_Index() const
    {
        return origin->getChangedID_Index();
    }

    //! Used by the VideoDriver to remember the buffer link.
    virtual void setHWBuffer(void *ptr) const
    {
        origin->setHWBuffer(ptr);
    }
    
    virtual void *getHWBuffer() const
    {
        return origin->getHWBuffer();
    }

    //! Describe what kind of primitive geometry is used by the meshbuffer
    /** Note: Default is EPT_TRIANGLES. Using other types is fine for rendering.
    But meshbuffer manipulation functions might expect type EPT_TRIANGLES
    to work correctly. Also mesh writers will generally fail (badly!) with other
    types than EPT_TRIANGLES. */
    virtual void setPrimitiveType(scene::E_PRIMITIVE_TYPE type)
    {
        origin->setPrimitiveType(type);
    }

    //! Get the kind of primitive geometry which is used by the meshbuffer
    virtual scene::E_PRIMITIVE_TYPE getPrimitiveType() const
    {
        return origin->getPrimitiveType();
    }

    //! Returns true is indices the buffer points to are 0-based
    bool hasLocalIndices();

    //! Converts to 0-base indices (drawing directly)
    void convertToLocalIndices();

    //! Converts to base_vertex indices (drawing via master mesh)
    void convertToGlobalIndices();

    //! Offsets the vertices in this buffer by a given offset
    void offsetBy(v3f offset);

private:
    scene::IMeshBuffer *origin;
    u16 vertex_base;
    u16 vertex_count;
    u16 index_base;
    u16 index_count;

    core::aabbox3df bounding_box;
};

class MultiMesh;

class ProxyMesh : public scene::IMesh
{
public:
    ProxyMesh(MultiMesh *_master_mesh);

    virtual ~ProxyMesh() override;

    //! Get the amount of mesh buffers.
    /** \return Amount of mesh buffers (IMeshBuffer) in this mesh. */
    virtual u32 getMeshBufferCount() const override
    {
        return buffers.size();
    }

    //! Get pointer to a mesh buffer.
    /** \param nr: Zero based index of the mesh buffer. The maximum value is
    getMeshBufferCount() - 1;
    \return Pointer to the mesh buffer or 0 if there is no such
    mesh buffer. */
    virtual ProxyMeshBuffer* getMeshBuffer(u32 nr) const override
    {
        if (nr >= buffers.size())
            return nullptr;

        return buffers[nr];
    }

    //! Get pointer to a mesh buffer which fits a material
    /** \param material: material to search for
    \return Pointer to the mesh buffer or 0 if there is no such
    mesh buffer. */
    virtual ProxyMeshBuffer* getMeshBuffer( const video::SMaterial &material) const override;

    //! Get an axis aligned bounding box of the mesh.
    /** \return Bounding box of this mesh. */
    virtual const core::aabbox3d<f32>& getBoundingBox() const override
    {
        return bounding_box;
    }

    //! Set user-defined axis aligned bounding box
    /** \param box New bounding box to use for the mesh. */
    virtual void setBoundingBox( const core::aabbox3df& box) override
    {
        bounding_box = box;
    }

    //! Sets a flag of all contained materials to a new value.
    /** \param flag: Flag to set in all materials.
    \param newvalue: New value to set in all materials. */
    virtual void setMaterialFlag(video::E_MATERIAL_FLAG flag, bool newvalue) override
    {
        for (auto buffer : buffers)
            buffer->getMaterial().setFlag(flag, newvalue);
    }

    //! Set the hardware mapping hint
    /** This methods allows to define optimization hints for the
    hardware. This enables, e.g., the use of hardware buffers on
    platforms that support this feature. This can lead to noticeable
    performance gains. */
    virtual void setHardwareMappingHint(scene::E_HARDWARE_MAPPING newMappingHint, scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX) override
    {
        for (auto b : buffers)
            b->setHardwareMappingHint(newMappingHint, buffer);
    }

    //! Flag the meshbuffer as changed, reloads hardware buffers
    /** This method has to be called every time the vertices or
    indices have changed. Otherwise, changes won't be updated
    on the GPU in the next render cycle. */
    virtual void setDirty(scene::E_BUFFER_TYPE buffer=scene::EBT_VERTEX_AND_INDEX) override
    {
        for (auto b : buffers)
            b->setDirty(buffer);
    }

    ProxyMeshBuffer *addBuffer(scene::IMeshBuffer *source_buffer, u16 vertex_base, u16 vertex_count, u16 index_base, u16 index_count)
    {
        auto buffer = new ProxyMeshBuffer(source_buffer, vertex_base, vertex_count, index_base, index_count);
        buffers.push_back(buffer);
        return buffer;
    }

    scene::SMesh *convertToMesh() const
    {
        auto mesh = new scene::SMesh();

        for (auto buffer : buffers)
        {
            auto target_buffer = new scene::SMeshBuffer();
            target_buffer->setPrimitiveType(buffer->getPrimitiveType());
            buffer->convertToLocalIndices();
            target_buffer->getMaterial() = buffer->getMaterial();
            target_buffer->append(buffer->getVertices(), buffer->getVertexCount(),
                    buffer->getIndices(), buffer->getIndexCount());
            mesh->addMeshBuffer(target_buffer);
            target_buffer->drop();
        }

        return mesh;
    }
private:
    std::vector<ProxyMeshBuffer*> buffers;
    core::aabbox3df bounding_box;
    scene::IMesh *master_mesh;
};


class MultiMesh : public scene::SMesh
{
public:
    void merge(scene::IMesh *mesh);
    void release(u16 buffer_index, u16 vertex_base, u16 vertex_count, u16 index_base, u16 index_count);
};
