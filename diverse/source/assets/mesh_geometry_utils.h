#pragma once

#include "cpu_assets.h"
#include <cstdint>

namespace diverse::mesh_geometry
{
    void generate_normals(Vertex* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);
    void generate_tangents_bitangents(Vertex* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);

    inline MeshStats compute_stats(const MeshAsset& mesh)
    {
        MeshStats stats;
        stats.vertex_count = static_cast<uint32_t>(mesh.get_vertex_count());
        stats.index_count = static_cast<uint32_t>(mesh.get_index_count());
        stats.triangle_count = stats.index_count / 3;
        return stats;
    }
}
