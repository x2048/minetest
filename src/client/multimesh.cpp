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

#include "multimesh.h"

void ProxyMeshBuffer::recalculateBoundingBox()
{
    bounding_box.reset(getPosition(0));
    for (int i = 1; i < vertex_count; i++)
        bounding_box.addInternalPoint(getPosition(i));
}

bool ProxyMeshBuffer::hasLocalIndices()
{
    u16 *indices = getIndices();
    for (int i = 0; i < index_count; i++)
        if (indices[i] >= vertex_count)
            return false;
    return true;
}

void ProxyMeshBuffer::convertToLocalIndices()
{
    if (hasLocalIndices())
        return;
    
    u16 *indices = getIndices();
    for (int i = 0; i < index_count; i++)
        indices[i] -= vertex_base;
}

void ProxyMeshBuffer::convertToGlobalIndices()
{
    if (!hasLocalIndices())
        return;
    
    u16 *indices = getIndices();
    for (int i = 0; i < index_count; i++)
        indices[i] += vertex_base;
}

void ProxyMeshBuffer::offsetBy(v3f offset)
{
    auto vertices = static_cast<video::S3DVertex*>(getVertices());
    for (auto p = vertices; p < vertices + vertex_count; p++)
        p->Pos += offset;
}

/// ProxyMesh

ProxyMesh::ProxyMesh(MultiMesh *_master_mesh)
    : master_mesh(_master_mesh)
{
    master_mesh->grab();
}

ProxyMesh::~ProxyMesh()
{
    for (auto buffer : buffers)
        buffer->drop();
    master_mesh->drop();
}


ProxyMeshBuffer* ProxyMesh::getMeshBuffer(const video::SMaterial &material) const
{
    for (auto buffer : buffers)
        if (buffer->getMaterial() == material)
            return buffer;
    return nullptr;
}

void MultiMesh::merge(scene::IMesh *source_mesh)
{
    for (u16 i = 0; i < source_mesh->getMeshBufferCount(); i++) {
        auto source_buffer = source_mesh->getMeshBuffer(i);
        assert(source_buffer != nullptr);

        u16 vertex_count = source_buffer->getVertexCount();
        u16 index_count = source_buffer->getIndexCount();

        auto target_buffer = getMeshBuffer(source_buffer->getMaterial());
        if (!target_buffer || 
                target_buffer->getVertexCount() > 0xFFFFu - vertex_count ||
                target_buffer->getIndexCount() > 0xFFFFu - index_count) {
            target_buffer = new scene::SMeshBuffer();
            target_buffer->setHardwareMappingHint(scene::EHM_STATIC);
            target_buffer->getMaterial() = source_buffer->getMaterial();
            addMeshBuffer(target_buffer);
            target_buffer->drop();
        }

        target_buffer->append(source_buffer->getVertices(), vertex_count,
                source_buffer->getIndices(), index_count);
    }
}
