#include "mesh_factory.h"
#include "core/profiler.h"
#include "core/ds_log.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>


namespace diverse
{
    Mesh* CreateQuad(float x, float y, float width, float height)
    {
        DS_PROFILE_FUNCTION();

        std::vector<Vertex> data(4);

        data[0].Position  = glm::vec3(x, y, 0.0f);
        data[0].TexCoords = glm::vec2(0.0f, 1.0f);

        data[1].Position  = glm::vec3(x + width, y, 0.0f);
        data[1].TexCoords = glm::vec2(0, 0);

        data[2].Position  = glm::vec3(x + width, y + height, 0.0f);
        data[2].TexCoords = glm::vec2(1, 0);

        data[3].Position  = glm::vec3(x, y + height, 0.0f);
        data[3].TexCoords = glm::vec2(1, 1);

        std::vector<uint32_t> indices = {
            0,
            1,
            2,
            2,
            3,
            0,
        };
        return new Mesh(indices, data);
    }

    Mesh* CreateQuad(const glm::vec2& position, const glm::vec2& size)
    {
        return CreateQuad(position.x, position.y, size.x, size.y);
    }

    Mesh* CreateQuad()
    {
        DS_PROFILE_FUNCTION();
        std::vector<Vertex> data(4);

        data[0].Position  = glm::vec3(-1.0f, -1.0f, 0.0f);
        data[0].Colours   = glm::vec4(1.0f);
        data[0].TexCoords = glm::vec2(0.0f, 0.0f);

        data[1].Position  = glm::vec3(1.0f, -1.0f, 0.0f);
        data[1].Colours   = glm::vec4(1.0f);
        data[1].TexCoords = glm::vec2(1.0f, 0.0f);

        data[2].Position  = glm::vec3(1.0f, 1.0f, 0.0f);
        data[2].Colours   = glm::vec4(1.0f);
        data[2].TexCoords = glm::vec2(1.0f, 1.0f);

        data[3].Position  = glm::vec3(-1.0f, 1.0f, 0.0f);
        data[3].Colours   = glm::vec4(1.0f);
        data[3].TexCoords = glm::vec2(0.0f, 1.0f);

        std::vector<uint32_t> indices = {
            0,
            1,
            2,
            2,
            3,
            0,
        };

        return new Mesh(indices, data);
    }

    Mesh* CreateCube()
    {
        //    v6----- v5
        //   /|      /|
        //  v1------v0|
        //  | |     | |
        //  | |v7---|-|v4
        //  |/      |/
        //  v2------v3
        DS_PROFILE_FUNCTION();
        std::vector<Vertex> data(24);

        data[0].Position = glm::vec3(0.5f, 0.5f, 0.5f);
        data[0].Colours  = glm::vec4(1.0f);
        data[0].Normal   = glm::vec3(0.0f, 0.0f, 1.0f);

        data[1].Position = glm::vec3(-0.5f, 0.5f, 0.5f);
        data[1].Colours  = glm::vec4(1.0f);
        data[1].Normal   = glm::vec3(0.0f, 0.0f, 1.0f);

        data[2].Position = glm::vec3(-0.5f, -0.5f, 0.5f);
        data[2].Colours  = glm::vec4(1.0f);
        data[2].Normal   = glm::vec3(0.0f, 0.0f, 1.0f);

        data[3].Position = glm::vec3(0.5f, -0.5f, 0.5f);
        data[3].Colours  = glm::vec4(1.0f);
        data[3].Normal   = glm::vec3(0.0f, 0.0f, 1.0f);

        data[4].Position = glm::vec3(0.5f, 0.5f, 0.5f);
        data[4].Colours  = glm::vec4(1.0f);
        data[4].Normal   = glm::vec3(1.0f, 0.0f, 0.0f);

        data[5].Position = glm::vec3(0.5f, -0.5f, 0.5f);
        data[5].Colours  = glm::vec4(1.0f);
        data[5].Normal   = glm::vec3(1.0f, 0.0f, 0.0f);

        data[6].Position = glm::vec3(0.5f, -0.5f, -0.5f);
        data[6].Colours  = glm::vec4(1.0f);
        data[6].Normal   = glm::vec3(1.0f, 0.0f, 0.0f);

        data[7].Position = glm::vec3(0.5f, 0.5f, -0.5f);
        data[7].Colours  = glm::vec4(1.0f);
        data[7].Normal   = glm::vec3(1.0f, 0.0f, 0.0f);

        data[8].Position = glm::vec3(0.5f, 0.5f, 0.5f);
        data[8].Colours  = glm::vec4(1.0f);
        data[8].Normal   = glm::vec3(0.0f, 1.0f, 0.0f);

        data[9].Position = glm::vec3(0.5f, 0.5f, -0.5f);
        data[9].Colours  = glm::vec4(1.0f);
        data[9].Normal   = glm::vec3(0.0f, 1.0f, 0.0f);

        data[10].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
        data[10].Colours  = glm::vec4(1.0f);
        data[10].Normal   = glm::vec3(0.0f, 1.0f, 0.0f);

        data[11].Position = glm::vec3(-0.5f, 0.5f, 0.5f);
        data[11].Colours  = glm::vec4(1.0f);
        data[11].Normal   = glm::vec3(0.0f, 1.0f, 0.0f);

        data[12].Position = glm::vec3(-0.5f, 0.5f, 0.5f);
        data[12].Colours  = glm::vec4(1.0f);
        data[12].Normal   = glm::vec3(-1.0f, 0.0f, 0.0f);

        data[13].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
        data[13].Colours  = glm::vec4(1.0f);
        data[13].Normal   = glm::vec3(-1.0f, 0.0f, 0.0f);

        data[14].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
        data[14].Colours  = glm::vec4(1.0f);
        data[14].Normal   = glm::vec3(-1.0f, 0.0f, 0.0f);

        data[15].Position = glm::vec3(-0.5f, -0.5f, 0.5f);
        data[15].Colours  = glm::vec4(1.0f);
        data[15].Normal   = glm::vec3(-1.0f, 0.0f, 0.0f);

        data[16].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
        data[16].Colours  = glm::vec4(1.0f);
        data[16].Normal   = glm::vec3(0.0f, -1.0f, 0.0f);

        data[17].Position = glm::vec3(0.5f, -0.5f, -0.5f);
        data[17].Colours  = glm::vec4(1.0f);
        data[17].Normal   = glm::vec3(0.0f, -1.0f, 0.0f);

        data[18].Position = glm::vec3(0.5f, -0.5f, 0.5f);
        data[18].Colours  = glm::vec4(1.0f);
        data[18].Normal   = glm::vec3(0.0f, -1.0f, 0.0f);

        data[19].Position = glm::vec3(-0.5f, -0.5f, 0.5f);
        data[19].Colours  = glm::vec4(1.0f);
        data[19].Normal   = glm::vec3(0.0f, -1.0f, 0.0f);

        data[20].Position = glm::vec3(0.5f, -0.5f, -0.5f);
        data[20].Colours  = glm::vec4(1.0f);
        data[20].Normal   = glm::vec3(0.0f, 0.0f, -1.0f);

        data[21].Position = glm::vec3(-0.5f, -0.5f, -0.5f);
        data[21].Colours  = glm::vec4(1.0f);
        data[21].Normal   = glm::vec3(0.0f, 0.0f, -1.0f);

        data[22].Position = glm::vec3(-0.5f, 0.5f, -0.5f);
        data[22].Colours  = glm::vec4(1.0f);
        data[22].Normal   = glm::vec3(0.0f, 0.0f, -1.0f);

        data[23].Position = glm::vec3(0.5f, 0.5f, -0.5f);
        data[23].Colours  = glm::vec4(1.0f);
        data[23].Normal   = glm::vec3(0.0f, 0.0f, -1.0f);

        for(int i = 0; i < 6; i++)
        {
            data[i * 4 + 0].TexCoords = glm::vec2(0.0f, 0.0f);
            data[i * 4 + 1].TexCoords = glm::vec2(1.0f, 0.0f);
            data[i * 4 + 2].TexCoords = glm::vec2(1.0f, 1.0f);
            data[i * 4 + 3].TexCoords = glm::vec2(0.0f, 1.0f);
        }

        std::vector<uint32_t> indices = {
            0, 1, 2,
            0, 2, 3,
            4, 5, 6,
            4, 6, 7,
            8, 9, 10,
            8, 10, 11,
            12, 13, 14,
            12, 14, 15,
            16, 17, 18,
            16, 18, 19,
            20, 21, 22,
            20, 22, 23
        };

        return new Mesh(indices, data);
    }

    Mesh* CreatePyramid()
    {
        DS_PROFILE_FUNCTION();
        std::vector<Vertex> data(18);

        data[0].Position  = glm::vec3(1.0f, 1.0f, -1.0f);
        data[0].Colours   = glm::vec4(1.0f);
        data[0].TexCoords = glm::vec2(0.24f, 0.20f);
        data[0].Normal    = glm::vec3(0.0f, 0.8948f, 0.4464f);

        data[1].Position  = glm::vec3(-1.0f, 1.0f, -1.0f);
        data[1].Colours   = glm::vec4(1.0f);
        data[1].TexCoords = glm::vec2(0.24f, 0.81f);
        data[1].Normal    = glm::vec3(0.0f, 0.8948f, 0.4464f);

        data[2].Position  = glm::vec3(0.0f, 0.0f, 1.0f);
        data[2].Colours   = glm::vec4(1.0f);
        data[2].TexCoords = glm::vec2(0.95f, 0.50f);
        data[2].Normal    = glm::vec3(0.0f, 0.8948f, 0.4464f);

        data[3].Position  = glm::vec3(-1.0f, 1.0f, -1.0f);
        data[3].Colours   = glm::vec4(1.0f);
        data[3].TexCoords = glm::vec2(0.24f, 0.21f);
        data[3].Normal    = glm::vec3(-0.8948f, 0.0f, 0.4464f);

        data[4].Position  = glm::vec3(-1.0f, -1.0f, -1.0f);
        data[4].Colours   = glm::vec4(1.0f);
        data[4].TexCoords = glm::vec2(0.24f, 0.81f);
        data[4].Normal    = glm::vec3(-0.8948f, 0.0f, 0.4464f);

        data[5].Position  = glm::vec3(0.0f, 0.0f, 1.0f);
        data[5].Colours   = glm::vec4(1.0f);
        data[5].TexCoords = glm::vec2(0.95f, 0.50f);
        data[5].Normal    = glm::vec3(-0.8948f, 0.0f, 0.4464f);

        data[6].Position  = glm::vec3(1.0f, 1.0f, -1.0f);
        data[6].Colours   = glm::vec4(1.0f);
        data[6].TexCoords = glm::vec2(0.24f, 0.81f);
        data[6].Normal    = glm::vec3(0.8948f, 0.0f, 0.4475f);

        data[7].Position  = glm::vec3(0.0f, 0.0f, 1.0f);
        data[7].Colours   = glm::vec4(1.0f);
        data[7].TexCoords = glm::vec2(0.95f, 0.50f);
        data[7].Normal    = glm::vec3(0.8948f, 0.0f, 0.4475f);

        data[8].Position  = glm::vec3(1.0f, -1.0f, -1.0f);
        data[8].Colours   = glm::vec4(1.0f);
        data[8].TexCoords = glm::vec2(0.24f, 0.21f);
        data[8].Normal    = glm::vec3(0.8948f, 0.0f, 0.4475f);

        data[9].Position  = glm::vec3(-1.0f, -1.0f, -1.0f);
        data[9].Colours   = glm::vec4(1.0f);
        data[9].TexCoords = glm::vec2(0.24f, 0.21f);
        data[9].Normal    = glm::vec3(0.0f, -0.8948f, 0.448f);

        data[10].Position  = glm::vec3(1.0f, -1.0f, -1.0f);
        data[10].Colours   = glm::vec4(1.0f);
        data[10].TexCoords = glm::vec2(0.24f, 0.81f);
        data[10].Normal    = glm::vec3(0.0f, -0.8948f, 0.448f);

        data[11].Position  = glm::vec3(0.0f, 0.0f, 1.0f);
        data[11].Colours   = glm::vec4(1.0f);
        data[11].TexCoords = glm::vec2(0.95f, 0.50f);
        data[11].Normal    = glm::vec3(0.0f, -0.8948f, 0.448f);

        data[12].Position  = glm::vec3(-1.0f, 1.0f, -1.0f);
        data[12].Colours   = glm::vec4(1.0f);
        data[12].TexCoords = glm::vec2(0.0f, 0.0f);
        data[12].Normal    = glm::vec3(0.0f, 0.0f, -1.0f);

        data[13].Position  = glm::vec3(1.0f, 1.0f, -1.0f);
        data[13].Colours   = glm::vec4(1.0f);
        data[13].TexCoords = glm::vec2(0.0f, 1.0f);
        data[13].Normal    = glm::vec3(0.0f, 0.0f, -1.0f);

        data[14].Position  = glm::vec3(1.0f, -1.0f, -1.0f);
        data[14].Colours   = glm::vec4(1.0f);
        data[14].TexCoords = glm::vec2(1.0f, 1.0f);
        data[14].Normal    = glm::vec3(0.0f, 0.0f, -1.0f);

        data[15].Position  = glm::vec3(-1.0f, -1.0f, -1.0f);
        data[15].Colours   = glm::vec4(1.0f);
        data[15].TexCoords = glm::vec2(0.96f, 0.0f);
        data[15].Normal    = glm::vec3(0.0f, 0.0f, -1.0f);

        data[16].Position  = glm::vec3(0.0f, 0.0f, 0.0f);
        data[16].Colours   = glm::vec4(1.0f);
        data[16].TexCoords = glm::vec2(0.0f, 0.0f);
        data[16].Normal    = glm::vec3(0.0f, 0.0f, 0.0f);

        data[17].Position  = glm::vec3(0.0f, 0.0f, 0.0f);
        data[17].Colours   = glm::vec4(1.0f);
        data[17].TexCoords = glm::vec2(0.0f, 0.0f);
        data[17].Normal    = glm::vec3(0.0f, 0.0f, 0.0f);

        std::vector<uint32_t> indices = {
            0, 1, 2,
            3, 4, 5,
            6, 7, 8,
            9, 10, 11,
            12, 13, 14,
            15, 12, 14
        };

        return new Mesh(indices, data);
    }

    Mesh* CreateSphere(uint32_t xSegments, uint32_t ySegments)
    {
        DS_PROFILE_FUNCTION();
        auto data = std::vector<Vertex>();

        float sectorCount = static_cast<float>(xSegments);
        float stackCount  = static_cast<float>(ySegments);
        float sectorStep  = 2 * maths::M_PI / sectorCount;
        float stackStep   = maths::M_PI / stackCount;
        float radius      = 0.5f;

        for(int i = 0; i <= stackCount; ++i)
        {
            float stackAngle = maths::M_PI / 2 - i * stackStep; // starting from pi/2 to -pi/2
            float xy         = radius * cos(stackAngle);        // r * cos(u)
            float z          = radius * sin(stackAngle);        // r * sin(u)

            // add (sectorCount+1) vertices per stack
            // the first and last vertices have same position and normal, but different tex coords
            for(int j = 0; j <= sectorCount; ++j)
            {
                float sectorAngle = j * sectorStep; // starting from 0 to 2pi

                // vertex position (x, y, z)
                float x = xy * cosf(sectorAngle); // r * cos(u) * cos(v)
                float y = xy * sinf(sectorAngle); // r * cos(u) * sin(v)

                // vertex tex coord (s, t) range between [0, 1]
                float s = static_cast<float>(j / sectorCount);
                float t = static_cast<float>(i / stackCount);

                Vertex vertex;
                vertex.Position  = glm::vec3(x, y, z);
                vertex.TexCoords = glm::vec2(s, t);
                vertex.Normal    = glm::normalize(glm::vec3(x, y, z));

                data.emplace_back(vertex);
            }
        }

        std::vector<uint32_t> indices;
        uint32_t k1, k2;
        for(uint32_t i = 0; i < stackCount; ++i)
        {
            k1 = i * (static_cast<uint32_t>(sectorCount) + 1U); // beginning of current stack
            k2 = k1 + static_cast<uint32_t>(sectorCount) + 1U;  // beginning of next stack

            for(uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2)
            {
                // 2 triangles per sector excluding first and last stacks
                // k1 => k2 => k1+1
                if(i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                // k1+1 => k2 => k2+1
                if(i != (stackCount - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        return new Mesh(indices, data);
    }

    Mesh* CreatePlane(float width, float height, const glm::vec3& normal)
    {
        DS_PROFILE_FUNCTION();
        glm::vec3 vec      = normal * 90.0f;
        glm::quat rotation = glm::quat(vec.z, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::quat(vec.y, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::quat(vec.x, glm::vec3(0.0f, 0.0f, 1.0f));

        std::vector<Vertex> data(4);

        data[0].Position  = rotation * glm::vec3(-width * 0.5f, -1.0f, -height * 0.5f);
        data[0].Normal    = normal;
        data[0].TexCoords = glm::vec2(0.0f, 0.0f);

        data[1].Position  = rotation * glm::vec3(-width * 0.5f, -1.0f, height * 0.5f);
        data[1].Normal    = normal;
        data[1].TexCoords = glm::vec2(0.0f, 1.0f);

        data[2].Position  = rotation * glm::vec3(width * 0.5f, 1.0f, height * 0.5f);
        data[2].Normal    = normal;
        data[2].TexCoords = glm::vec2(1.0f, 1.0f);

        data[3].Position  = rotation * glm::vec3(width * 0.5f, 1.0f, -height * 0.5f);
        data[3].Normal    = normal;
        data[3].TexCoords = glm::vec2(1.0f, 0.0f);

        std::vector<uint32_t> indices = {
            0, 1, 2,
            2, 3, 0
        };
        return new Mesh(indices, data);
    }

    Mesh* CreateCapsule(float radius, float midHeight, int radialSegments, int rings)
        {
            DS_PROFILE_FUNCTION();
            int i, j, prevrow, thisrow, point;
            float x, y, z, u, v, w;
            float onethird  = 1.0f / 3.0f;
            float twothirds = 2.0f / 3.0f;

            std::vector<Vertex> data;
            std::vector<uint32_t> indices;

            point = 0;

            /* top hemisphere */
            thisrow = 0;
            prevrow = 0;
            for(j = 0; j <= (rings + 1); j++)
            {
                v = float(j);

                v /= (rings + 1);
                w = sin(0.5f * maths::M_PI * v);
                y = radius * cos(0.5f * maths::M_PI * v);

                for(i = 0; i <= radialSegments; i++)
                {
                    u = float(i);
                    u /= radialSegments;

                    x = sin(u * (maths::M_PI * 2.0f));
                    z = cos(u * (maths::M_PI * 2.0f));

                    glm::vec3 p = glm::vec3(x * radius * w, y, z * radius * w);

                    Vertex vertex;
                    vertex.Position  = p + glm::vec3(0.0f, 0.5f * midHeight, 0.0f);
                    vertex.Normal    = glm::normalize(p);
                    vertex.TexCoords = glm::vec2(u, onethird * v);
                    data.emplace_back(vertex);
                    point++;

                    if(i > 0 && j > 0)
                    {
                        indices.push_back(thisrow + i - 1);
                        indices.push_back(prevrow + i);
                        indices.push_back(prevrow + i - 1);

                        indices.push_back(thisrow + i - 1);
                        indices.push_back(thisrow + i);
                        indices.push_back(prevrow + i);
                    };
                };

                prevrow = thisrow;
                thisrow = point;
            };

            /* cylinder */
            thisrow = point;
            prevrow = 0;
            for(j = 0; j <= (rings + 1); j++)
            {
                v = float(j);
                v /= (rings + 1);

                y = midHeight * v;
                y = (midHeight * 0.5f) - y;

                for(i = 0; i <= radialSegments; i++)
                {
                    u = float(i);
                    u /= radialSegments;

                    x = sin(u * (maths::M_PI * 2.0f));
                    z = cos(u * (maths::M_PI * 2.0f));

                    glm::vec3 p = glm::vec3(x * radius, y, z * radius);

                    Vertex vertex;
                    vertex.Position = p;
                    // vertex.Normal = glm::vec3(x, z, 0.0f);
                    vertex.Normal = glm::vec3(x, 0.0f, z);
                    // vertex.TexCoords = glm::vec2(u, onethird + (v * onethird));
                    vertex.TexCoords = glm::vec2(u, v * 0.5f);
                    data.emplace_back(vertex);

                    point++;

                    if(i > 0 && j > 0)
                    {
                        indices.push_back(thisrow + i - 1);
                        indices.push_back(prevrow + i);
                        indices.push_back(prevrow + i - 1);

                        indices.push_back(thisrow + i - 1);
                        indices.push_back(thisrow + i);
                        indices.push_back(prevrow + i);
                    };
                };

                prevrow = thisrow;
                thisrow = point;
            };

            /* bottom hemisphere */
            thisrow = point;
            prevrow = 0;

            for(j = 0; j <= (rings + 1); j++)
            {
                v = float(j);

                v /= (rings + 1);
                v += 1.0f;
                w = sin(0.5f * maths::M_PI * v);
                y = radius * cos(0.5f * maths::M_PI * v);

                for(i = 0; i <= radialSegments; i++)
                {
                    float u2 = float(i);
                    u2 /= radialSegments;

                    x = sin(u2 * (maths::M_PI * 2.0f));
                    z = cos(u2 * (maths::M_PI * 2.0f));

                    glm::vec3 p = glm::vec3(x * radius * w, y, z * radius * w);

                    Vertex vertex;
                    vertex.Position  = p + glm::vec3(0.0f, -0.5f * midHeight, 0.0f);
                    vertex.Normal    = glm::normalize(p);
                    vertex.TexCoords = glm::vec2(u2, twothirds + ((v - 1.0f) * onethird));
                    data.emplace_back(vertex);

                    point++;

                    if(i > 0 && j > 0)
                    {
                        indices.push_back(thisrow + i - 1);
                        indices.push_back(prevrow + i);
                        indices.push_back(prevrow + i - 1);

                        indices.push_back(thisrow + i - 1);
                        indices.push_back(thisrow + i);
                        indices.push_back(prevrow + i);
                    };
                };

                prevrow = thisrow;
                thisrow = point;
            }

            return new Mesh(indices, data);
        }

    Mesh* CreateCylinder(float bottomRadius, float topRadius, float height, int radialSegments, int rings)
    {
        DS_PROFILE_FUNCTION();
        int i, j, prevrow, thisrow, point = 0;
        float x, y, z, u, v, radius;

        std::vector<Vertex> data;
        std::vector<uint32_t> indices;

        thisrow = 0;
        prevrow = 0;
        for (j = 0; j <= (rings + 1); j++)
        {
            v = float(j);
            v /= (rings + 1);

            radius = topRadius + ((bottomRadius - topRadius) * v);

            y = height * v;
            y = (height * 0.5f) - y;

            for (i = 0; i <= radialSegments; i++)
            {
                u = float(i);
                u /= radialSegments;

                x = sin(u * (maths::M_PI * 2.0f));
                z = cos(u * (maths::M_PI * 2.0f));

                glm::vec3 p = glm::vec3(x * radius, y, z * radius);

                Vertex vertex;
                vertex.Position = p;
                vertex.Normal = glm::vec3(x, 0.0f, z);
                vertex.TexCoords = glm::vec2(u, v * 0.5f);
                data.emplace_back(vertex);

                point++;

                if (i > 0 && j > 0)
                {
                    indices.push_back(thisrow + i - 1);
                    indices.push_back(prevrow + i);
                    indices.push_back(prevrow + i - 1);

                    indices.push_back(thisrow + i - 1);
                    indices.push_back(thisrow + i);
                    indices.push_back(prevrow + i);
                };
            };

            prevrow = thisrow;
            thisrow = point;
        };

        // add top
        if (topRadius > 0.0f)
        {
            y = height * 0.5f;

            Vertex vertex;
            vertex.Position = glm::vec3(0.0f, y, 0.0f);
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.TexCoords = glm::vec2(0.25f, 0.75f);
            data.emplace_back(vertex);
            point++;

            for (i = 0; i <= radialSegments; i++)
            {
                float r = float(i);
                r /= radialSegments;

                x = sin(r * (maths::M_PI * 2.0f));
                z = cos(r * (maths::M_PI * 2.0f));

                u = ((x + 1.0f) * 0.25f);
                v = 0.5f + ((z + 1.0f) * 0.25f);

                glm::vec3 p = glm::vec3(x * topRadius, y, z * topRadius);
                Vertex vertex;
                vertex.Position = p;
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                vertex.TexCoords = glm::vec2(u, v);
                data.emplace_back(vertex);
                point++;

                if (i > 0)
                {
                    indices.push_back(point - 2);
                    indices.push_back(point - 1);
                    indices.push_back(thisrow);
                };
            };
        };

        // add bottom
        if (bottomRadius > 0.0f)
        {
            y = height * -0.5f;

            thisrow = point;

            Vertex vertex;
            vertex.Position = glm::vec3(0.0f, y, 0.0f);
            vertex.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
            vertex.TexCoords = glm::vec2(0.75f, 0.75f);
            data.emplace_back(vertex);
            point++;

            for (i = 0; i <= radialSegments; i++)
            {
                float r = float(i);
                r /= radialSegments;

                x = sin(r * (maths::M_PI * 2.0f));
                z = cos(r * (maths::M_PI * 2.0f));

                u = 0.5f + ((x + 1.0f) * 0.25f);
                v = 1.0f - ((z + 1.0f) * 0.25f);

                glm::vec3 p = glm::vec3(x * bottomRadius, y, z * bottomRadius);

                vertex.Position = p;
                vertex.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
                vertex.TexCoords = glm::vec2(u, v);
                data.emplace_back(vertex);
                point++;

                if (i > 0)
                {
                    indices.push_back(point - 1);
                    indices.push_back(point - 2);
                    indices.push_back(thisrow);
                };
            };
        };

        return new Mesh(indices, data);
    }

    Mesh* CreatePrimative(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::Cube:
            return CreateCube();
        case PrimitiveType::Plane:
            return CreatePlane(1.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        case PrimitiveType::Quad:
            return CreateQuad();
        case PrimitiveType::Sphere:
            return CreateSphere();
        case PrimitiveType::Pyramid:
            return CreatePyramid();
        case PrimitiveType::Capsule:
            return CreateCapsule();
        case PrimitiveType::Cylinder:
            return CreateCylinder();
        //case PrimitiveType::Terrain:
            //return CreateTerrain();
        }

        DS_LOG_ERROR("Primitive not supported");
        return nullptr;
    }
}