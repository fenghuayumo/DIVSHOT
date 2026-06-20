#include "mesh_geometry_utils.h"
#include "maths/maths_utils.h"
#include "utility/thread_pool.h"
#include <glm/glm.hpp>

namespace diverse::mesh_geometry
{
    void generate_normals(Vertex* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
    {
        std::vector<glm::vec3> normals(vertex_count);
        if (indices)
        {
            const auto num_triangles = index_count / 3;
            parallel_for<size_t>(0, num_triangles, [&](size_t i) {
                const int a = indices[i * 3];
                const int b = indices[i * 3 + 1];
                const int c = indices[i * 3 + 2];

                const glm::vec3 normal = glm::cross(
                    (vertices[b].Position - vertices[a].Position),
                    (vertices[c].Position - vertices[a].Position));

                normals[a] = normal;
                normals[b] = normal;
                normals[c] = normal;
            });
        }
        else
        {
            const auto num_faces = vertex_count / 3;
            parallel_for<size_t>(0, num_faces, [&](size_t i) {
                glm::vec3& a = vertices[i * 3 + 0].Position;
                glm::vec3& b = vertices[i * 3 + 1].Position;
                glm::vec3& c = vertices[i * 3 + 2].Position;

                const glm::vec3 normal = glm::cross(b - a, c - a);

                normals[i * 3 + 0] = normal;
                normals[i * 3 + 1] = normal;
                normals[i * 3 + 2] = normal;
            });
        }

        parallel_for<size_t>(0, vertex_count, [&](size_t i) {
            vertices[i].Normal = glm::normalize(normals[i]);
        });
    }

    void generate_tangents_bitangents(Vertex* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t num_indices)
    {
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            vertices[i].Tangent = glm::vec3(0.0f);
            vertices[i].Bitangent = glm::vec3(0.0f);
        }

        for (uint32_t i = 0; i < num_indices; i += 3)
        {
            const glm::vec3 v0 = vertices[indices[i]].Position;
            const glm::vec3 v1 = vertices[indices[i + 1]].Position;
            const glm::vec3 v2 = vertices[indices[i + 2]].Position;

            const glm::vec2 uv0 = vertices[indices[i]].TexCoords;
            const glm::vec2 uv1 = vertices[indices[i + 1]].TexCoords;
            const glm::vec2 uv2 = vertices[indices[i + 2]].TexCoords;

            const glm::vec3 edge1 = v1 - v0;
            const glm::vec3 edge2 = v2 - v0;

            const glm::vec2 delta_uv1 = uv1 - uv0;
            const glm::vec2 delta_uv2 = uv2 - uv0;

            float den = (delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y);
            if (den < maths::M_EPSILON)
                den = 1.0f;

            const float f = 1.0f / den;

            const glm::vec3 tangent = f * (delta_uv2.y * edge1 - delta_uv1.y * edge2);
            const glm::vec3 bitangent = f * (-delta_uv2.x * edge1 + delta_uv1.x * edge2);

            vertices[indices[i]].Tangent += tangent;
            vertices[indices[i + 1]].Tangent += tangent;
            vertices[indices[i + 2]].Tangent += tangent;

            vertices[indices[i]].Bitangent += bitangent;
            vertices[indices[i + 1]].Bitangent += bitangent;
            vertices[indices[i + 2]].Bitangent += bitangent;
        }

        for (uint32_t i = 0; i < vertex_count; i++)
        {
            if (glm::dot(vertices[i].Tangent, vertices[i].Tangent) > maths::M_EPSILON)
                vertices[i].Tangent = glm::normalize(vertices[i].Tangent);
            else
                vertices[i].Tangent = glm::vec3(0.0f);

            if (glm::dot(vertices[i].Bitangent, vertices[i].Bitangent) > maths::M_EPSILON)
                vertices[i].Bitangent = glm::normalize(vertices[i].Bitangent);
            else
                vertices[i].Bitangent = glm::vec3(0.0f);
        }
    }
}
