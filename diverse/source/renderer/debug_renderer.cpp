#include "core/profiler.h"
#include "maths/maths_utils.h"
#include "debug_renderer.h"
#include "core/ds_log.h"
#include <stdarg.h>
#include <iostream>
namespace diverse
{
        DebugRenderer* DebugRenderer::s_Instance = nullptr;

    static const uint32_t MaxLines        = 10000;
    static const uint32_t MaxLineVertices = MaxLines * 2;
    static const uint32_t MaxLineIndices  = MaxLines * 6;
#define MAX_BATCH_DRAW_CALLS 100
#define RENDERER_LINE_SIZE RENDERER2DLINE_VERTEX_SIZE * 4
#define RENDERER_BUFFER_SIZE RENDERER_LINE_SIZE* MaxLineVertices

#ifdef DS_PLATFORM_WINDOWS
#define VSNPRINTF(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList) vsnprintf_s(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList)
#elif DS_PLATFORM_MACOS
#define VSNPRINTF(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList) vsnprintf_l(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList)
#elif DS_PLATFORM_LINUX
#define VSNPRINTF(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList) vsnprintf(_DstBuf, _DstSize, _Format, _ArgList)
#elif DS_PLATFORM_MOBILE
#define VSNPRINTF(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList) 0
#else
#define VSNPRINTF(_DstBuf, _DstSize, _MaxCount, _Format, _ArgList) 0
#endif

#ifndef DS_PLATFORM_WINDOWS
#define _TRUNCATE 0
#endif

    void DebugRenderer::init()
    {
        if(s_Instance)
            return;

        s_Instance = new DebugRenderer();
    }

    void DebugRenderer::release()
    {
        DS_PROFILE_FUNCTION();
        delete s_Instance;
        s_Instance = nullptr;
    }

    void DebugRenderer::reset()
    {
        DS_PROFILE_FUNCTION();
        if(!s_Instance)
            return;

        s_Instance->m_DrawList.m_DebugTriangles.clear();
        s_Instance->m_DrawList.m_DebugLines.clear();
        s_Instance->m_DrawList.m_DebugThickLines.clear();
        s_Instance->m_DrawList.m_DebugPoints.clear();

        s_Instance->m_DrawListNDT.m_DebugTriangles.clear();
        s_Instance->m_DrawListNDT.m_DebugLines.clear();
        s_Instance->m_DrawListNDT.m_DebugThickLines.clear();
        s_Instance->m_DrawListNDT.m_DebugPoints.clear();

        s_Instance->m_TextList.clear();
        s_Instance->m_TextListNDT.clear();
        s_Instance->m_TextListCS.clear();
        s_Instance->m_NumStatusEntries    = 0;
        s_Instance->m_MaxStatusEntryWidth = 0.0f;
    }

    DebugDrawFrame DebugRenderer::capture_frame()
    {
        DS_PROFILE_FUNCTION();
        DebugDrawFrame frame;
        if (!s_Instance)
            return frame;

        frame.triangles[0] = s_Instance->m_DrawListNDT.m_DebugTriangles;
        frame.lines[0] = s_Instance->m_DrawListNDT.m_DebugLines;
        frame.thick_lines[0] = s_Instance->m_DrawListNDT.m_DebugThickLines;
        frame.points[0] = s_Instance->m_DrawListNDT.m_DebugPoints;

        frame.triangles[1] = s_Instance->m_DrawList.m_DebugTriangles;
        frame.lines[1] = s_Instance->m_DrawList.m_DebugLines;
        frame.thick_lines[1] = s_Instance->m_DrawList.m_DebugThickLines;
        frame.points[1] = s_Instance->m_DrawList.m_DebugPoints;
        return frame;
    }

    void DebugRenderer::clear_log_entries()
    {
        if(!s_Instance)
            return;

        s_Instance->m_vLogEntries.clear();
        s_Instance->m_LogEntriesOffset = 0;
    }

    void DebugRenderer::sort_lists()
    {
        if(!s_Instance)
            return;

        float cs_size_x = LOG_TEXT_SIZE / s_Instance->m_Width * 2.0f;
        float cs_size_y = LOG_TEXT_SIZE / s_Instance->m_Height * 2.0f;
        size_t log_len  = s_Instance->m_vLogEntries.size();

        float max_x = 0.0f;
        for(size_t i = 0; i < log_len; ++i)
        {
            max_x = maths::Max(max_x, s_Instance->m_vLogEntries[i].text.length() * cs_size_x * 0.6f);

            size_t idx                              = (i + s_Instance->m_LogEntriesOffset) % MAX_LOG_SIZE;
            float alpha                             = 1.0f - ((float)log_len - (float)i) / (float)log_len;
            s_Instance->m_vLogEntries[idx].colour.w = alpha;
            float aspect                            = (float)s_Instance->m_Width / (float)s_Instance->m_Height;
            draw_text_cs(glm::vec4(-aspect, -1.0f + ((log_len - i - 1) * cs_size_y) + cs_size_y, 0.0f, 1.0f), LOG_TEXT_SIZE, s_Instance->m_vLogEntries[idx].text, s_Instance->m_vLogEntries[idx].colour);
        }
    }

    DebugRenderer::DebugRenderer()
    {
        m_vLogEntries.clear();
        m_LogEntriesOffset = 0;
    }

    DebugRenderer::~DebugRenderer()
    {
    }

    // Draw Point (circle)
    void DebugRenderer::gen_draw_point(bool ndt, const glm::vec3& pos, float point_radius, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        if(ndt)
            s_Instance->m_DrawListNDT.m_DebugPoints.emplace_back(pos, point_radius, colour);
        else
            s_Instance->m_DrawList.m_DebugPoints.emplace_back(pos, point_radius, colour);
    }

    void DebugRenderer::draw_point(const glm::vec3& pos, float point_radius, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_point(false, pos, point_radius, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_point(const glm::vec3& pos, float point_radius, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_point(false, pos, point_radius, colour);
    }
    void DebugRenderer::draw_point_ndt(const glm::vec3& pos, float point_radius, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_point(true, pos, point_radius, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_point_ndt(const glm::vec3& pos, float point_radius, const glm::vec4& colour)
    {
        gen_draw_point(true, pos, point_radius, colour);
    }

    // Draw Line with a given thickness
    void DebugRenderer::gen_draw_thick_line(bool ndt, const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        if(ndt)
            s_Instance->m_DrawListNDT.m_DebugThickLines.emplace_back(start, end, colour);
        else
            s_Instance->m_DrawList.m_DebugThickLines.emplace_back(start, end, colour);
    }
    void DebugRenderer::draw_thick_line(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_thick_line(false, start, end, line_width, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_thick_line(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_thick_line(false, start, end, line_width, colour);
    }
    void DebugRenderer::draw_thick_line_ndt(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_thick_line(true, start, end, line_width, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_thick_line_ndt(const glm::vec3& start, const glm::vec3& end, float line_width, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_thick_line(true, start, end, line_width, colour);
    }

    // Draw line with thickness of 1 screen pixel regardless of distance from camera
    void DebugRenderer::gen_draw_hair_line(bool ndt, const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        if(ndt)
            s_Instance->m_DrawListNDT.m_DebugLines.emplace_back(start, end, colour);
        else
            s_Instance->m_DrawList.m_DebugLines.emplace_back(start, end, colour);
    }
    void DebugRenderer::draw_hair_line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(false, start, end, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_hair_line(const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(false, start, end, colour);
    }
    void DebugRenderer::draw_hair_line_ndt(const glm::vec3& start, const glm::vec3& end, const glm::vec3& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(true, start, end, glm::vec4(colour, 1.0f));
    }
    void DebugRenderer::draw_hair_line_ndt(const glm::vec3& start, const glm::vec3& end, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(true, start, end, colour);
    }

    // Draw Matrix (x,y,z axis at pos)
    void DebugRenderer::draw_matrix(const glm::mat4& mtx)
    {
        DS_PROFILE_FUNCTION();
        // glm::vec3 position = mtx[3];
        // gen_draw_hair_line(false, position, position + glm::vec3(mtx[0], mtx[1], mtx[2]), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        // gen_draw_hair_line(false, position, position + glm::vec3(mtx[4], mtx[5], mtx[6]), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        // gen_draw_hair_line(false, position, position + glm::vec3(mtx[8], mtx[9], mtx[10]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }
    void DebugRenderer::draw_matrix(const glm::mat3& mtx, const glm::vec3& position)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(false, position, position + mtx[0], glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        gen_draw_hair_line(false, position, position + mtx[1], glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        gen_draw_hair_line(false, position, position + mtx[2], glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }
    void DebugRenderer::draw_matrix_ndt(const glm::mat4& mtx)
    {
        DS_PROFILE_FUNCTION();
        // glm::vec3 position = mtx[3];
        // gen_draw_hair_line(true, position, position + glm::vec3(mtx[0], mtx[1], mtx[2]), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        // gen_draw_hair_line(true, position, position + glm::vec3(mtx[4], mtx[5], mtx[6]), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        // gen_draw_hair_line(true, position, position + glm::vec3(mtx[8], mtx[9], mtx[10]), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }
    void DebugRenderer::draw_matrix_ndt(const glm::mat3& mtx, const glm::vec3& position)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_hair_line(true, position, position + mtx[0], glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        gen_draw_hair_line(true, position, position + mtx[1], glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        gen_draw_hair_line(true, position, position + mtx[2], glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    }

    // Draw Triangle
    void DebugRenderer::gen_draw_triangle(bool ndt, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        if(ndt)
            s_Instance->m_DrawListNDT.m_DebugTriangles.emplace_back(v0, v1, v2, colour);
        else
            s_Instance->m_DrawList.m_DebugTriangles.emplace_back(v0, v1, v2, colour);
    }

    void DebugRenderer::draw_triangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_triangle(false, v0, v1, v2, colour);
    }

    void DebugRenderer::draw_triangle_ndt(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        gen_draw_triangle(true, v0, v1, v2, colour);
    }

    // Draw Polygon (Renders as a triangle fan, so verts must be arranged in order)
    void DebugRenderer::draw_polygon(int n_verts, const glm::vec3* verts, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        for(int i = 2; i < n_verts; ++i)
        {
            gen_draw_triangle(false, verts[0], verts[i - 1], verts[i], colour);
        }
    }

    void DebugRenderer::draw_polygon_ndt(int n_verts, const glm::vec3* verts, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        for(int i = 2; i < n_verts; ++i)
        {
            gen_draw_triangle(true, verts[0], verts[i - 1], verts[i], colour);
        }
    }

    void DebugRenderer::draw_text_cs(const glm::vec4& cs_pos, const float font_size, const std::string& text, const glm::vec4& colour)
    {
        glm::vec3 cs_size = glm::vec3(font_size / get_instance()->m_Width, font_size / get_instance()->m_Height, 0.0f);
        cs_size           = cs_size * cs_pos.w;

        // Work out the starting position of text based off desired alignment
        float x_offset      = 0.0f;
        const auto text_len = static_cast<int>(text.length());

        DebugText& dText = get_instance()->m_TextListCS.emplace_back();
        dText.text       = text;
        dText.Position   = cs_pos;
        dText.colour     = colour;
        dText.Size       = font_size;

        // Add each characters to the draw list individually
        // for (int i = 0; i < text_len; ++i)
        //{
        //     glm::vec4 char_pos = glm::vec4(cs_pos.x + x_offset, cs_pos.y, cs_pos.z, cs_pos.w);
        //     glm::vec4 char_data = glm::vec4(cs_size.x, cs_size.y, static_cast<float>(text[i]), 0.0f);

        //    get_instance()->m_vChars.push_back(char_pos);
        //    get_instance()->m_vChars.push_back(char_data);
        //    get_instance()->m_vChars.push_back(colour);
        //    get_instance()->m_vChars.push_back(colour);    //We dont really need this, but we need the padding to match the same vertex format as all the other debug drawables

        //    x_offset += cs_size.x * 1.2f;
        //}
    }

    // Draw Text WorldSpace
    void DebugRenderer::draw_text_ws(const glm::vec3& pos, const float font_size, const glm::vec4& colour, const std::string text, ...)
    {
        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;

        std::string formatted_text = std::string(buf, static_cast<size_t>(length));

        // glm::vec4 cs_pos = get_instance()->m_ProjViewMtx * glm::vec4(pos, 1.0f);
        // draw_text_cs(cs_pos, font_size, formatted_text, colour);

        DebugText& dText = get_instance()->m_TextList.emplace_back();
        dText.text       = formatted_text;
        dText.Position   = glm::vec4(pos, 1.0f);
        dText.colour     = colour;
        dText.Size       = font_size;
    }

    void DebugRenderer::draw_text_ws_ndt(const glm::vec3& pos, const float font_size, const glm::vec4& colour, const std::string text, ...)
    {
        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;

        std::string formatted_text = std::string(buf, static_cast<size_t>(length));

        // glm::vec4 cs_pos = get_instance()->m_ProjViewMtx * glm::vec4(pos, 1.0f);
        // cs_pos.z = (1.0f * cs_pos.w);
        // draw_text_cs(cs_pos, font_size, formatted_text, colour);

        DebugText& dText = get_instance()->m_TextListNDT.emplace_back();
        dText.text       = formatted_text;
        dText.Position   = glm::vec4(pos, 1.0f);
        dText.colour     = colour;
        dText.Size       = font_size;
    }

    // Status Entry
    void DebugRenderer::add_status_entry(const glm::vec4& colour, const std::string text, ...)
    {
        float cs_size_x = STATUS_TEXT_SIZE / get_instance()->m_Width * 2.0f;
        float cs_size_y = STATUS_TEXT_SIZE / get_instance()->m_Height * 2.0f;

        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;

        std::string formatted_text = std::string(buf, static_cast<size_t>(length));

        draw_text_cs(glm::vec4(-1.0f + cs_size_x * 0.5f, 1.0f - (get_instance()->m_NumStatusEntries * cs_size_y) + cs_size_y, -1.0f, 1.0f), STATUS_TEXT_SIZE, formatted_text, colour);
        get_instance()->m_NumStatusEntries++;
        get_instance()->m_MaxStatusEntryWidth = maths::Max(get_instance()->m_MaxStatusEntryWidth, cs_size_x * 0.6f * length);
    }

    // Log

    void DebugRenderer::add_log_entry(const glm::vec3& colour, const std::string& text)
    {
        /*    time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);*/

        // std::stringstream ss;
        // ss << "[" << ltm.tm_hour << ":" << ltm.tm_min << ":" << ltm.tm_sec << "] ";

        LogEntry le;
        le.text   = /*ss.str() + */ text; // +"\n";
        le.colour = glm::vec4(colour.x, colour.y, colour.z, 1.0f);

        if(get_instance()->m_vLogEntries.size() < MAX_LOG_SIZE)
            get_instance()->m_vLogEntries.push_back(le);
        else
        {
            get_instance()->m_vLogEntries[get_instance()->m_LogEntriesOffset] = le;
            get_instance()->m_LogEntriesOffset                               = (get_instance()->m_LogEntriesOffset + 1) % MAX_LOG_SIZE;
        }

        DS_LOG_WARN(text);
    }

    void DebugRenderer::log(const glm::vec3& colour, const std::string text, ...)
    {
        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;
        add_log_entry(colour, std::string(buf, static_cast<size_t>(length)));
    }

    void DebugRenderer::log(const std::string text, ...)
    {
        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;
        add_log_entry(glm::vec3(0.4f, 1.0f, 0.6f), std::string(buf, static_cast<size_t>(length)));
    }

    void DebugRenderer::log_e(const char* filename, int linenumber, const std::string text, ...)
    {
        // Error Format:
        //<text>
        //         -> <line number> : <file name>

        va_list args;
        va_start(args, text);

        char buf[1024];

        int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

        va_end(args);

        int length = (needed < 0) ? 1024 : needed;

        log(glm::vec3(1.0f, 0.25f, 0.25f), "[ERROR] %s:%d", filename, linenumber);
        add_log_entry(glm::vec3(1.0f, 0.5f, 0.5f), "\t \x01 \"" + std::string(buf, static_cast<size_t>(length)) + "\"");

        std::cout << std::endl;
    }

    void DebugRenderer::debug_draw(const maths::BoundingBox& box, const glm::vec4& edgeColour, bool cornersOnly, float width)
    {
        DS_PROFILE_FUNCTION();
        glm::vec3 uuu = box.max();
        glm::vec3 lll = box.min();

        glm::vec3 ull(uuu.x, lll.y, lll.z);
        glm::vec3 uul(uuu.x, uuu.y, lll.z);
        glm::vec3 ulu(uuu.x, lll.y, uuu.z);

        glm::vec3 luu(lll.x, uuu.y, uuu.z);
        glm::vec3 llu(lll.x, lll.y, uuu.z);
        glm::vec3 lul(lll.x, uuu.y, lll.z);

        // Draw edges
        if(!cornersOnly)
        {
            draw_thick_line_ndt(luu, uuu, width, edgeColour);
            draw_thick_line_ndt(lul, uul, width, edgeColour);
            draw_thick_line_ndt(llu, ulu, width, edgeColour);
            draw_thick_line_ndt(lll, ull, width, edgeColour);

            draw_thick_line_ndt(lul, lll, width, edgeColour);
            draw_thick_line_ndt(uul, ull, width, edgeColour);
            draw_thick_line_ndt(luu, llu, width, edgeColour);
            draw_thick_line_ndt(uuu, ulu, width, edgeColour);

            draw_thick_line_ndt(lll, llu, width, edgeColour);
            draw_thick_line_ndt(ull, ulu, width, edgeColour);
            draw_thick_line_ndt(lul, luu, width, edgeColour);
            draw_thick_line_ndt(uul, uuu, width, edgeColour);
        }
        else
        {
            draw_thick_line_ndt(luu, luu + (uuu - luu) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(luu + (uuu - luu) * 0.75f, uuu, width, edgeColour);

            draw_thick_line_ndt(lul, lul + (uul - lul) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(lul + (uul - lul) * 0.75f, uul, width, edgeColour);

            draw_thick_line_ndt(llu, llu + (ulu - llu) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(llu + (ulu - llu) * 0.75f, ulu, width, edgeColour);

            draw_thick_line_ndt(lll, lll + (ull - lll) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(lll + (ull - lll) * 0.75f, ull, width, edgeColour);

            draw_thick_line_ndt(lul, lul + (lll - lul) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(lul + (lll - lul) * 0.75f, lll, width, edgeColour);

            draw_thick_line_ndt(uul, uul + (ull - uul) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(uul + (ull - uul) * 0.75f, ull, width, edgeColour);

            draw_thick_line_ndt(luu, luu + (llu - luu) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(luu + (llu - luu) * 0.75f, llu, width, edgeColour);

            draw_thick_line_ndt(uuu, uuu + (ulu - uuu) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(uuu + (ulu - uuu) * 0.75f, ulu, width, edgeColour);

            draw_thick_line_ndt(lll, lll + (llu - lll) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(lll + (llu - lll) * 0.75f, llu, width, edgeColour);

            draw_thick_line_ndt(ull, ull + (ulu - ull) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(ull + (ulu - ull) * 0.75f, ulu, width, edgeColour);

            draw_thick_line_ndt(lul, lul + (luu - lul) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(lul + (luu - lul) * 0.75f, luu, width, edgeColour);

            draw_thick_line_ndt(uul, uul + (uuu - uul) * 0.25f, width, edgeColour);
            draw_thick_line_ndt(uul + (uuu - uul) * 0.75f, uuu, width, edgeColour);
        }
    }

    void DebugRenderer::debug_draw(const maths::BoundingSphere& sphere, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        diverse::DebugRenderer::draw_point_ndt(sphere.get_center(), sphere.get_radius(), colour);
    }

    void DebugRenderer::debug_draw(maths::Frustum& frustum, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        auto* vertices = frustum.get_verticies();

        DebugRenderer::draw_hair_line(vertices[0], vertices[1], colour);
        DebugRenderer::draw_hair_line(vertices[1], vertices[2], colour);
        DebugRenderer::draw_hair_line(vertices[2], vertices[3], colour);
        DebugRenderer::draw_hair_line(vertices[3], vertices[0], colour);
        DebugRenderer::draw_hair_line(vertices[4], vertices[5], colour);
        DebugRenderer::draw_hair_line(vertices[5], vertices[6], colour);
        DebugRenderer::draw_hair_line(vertices[6], vertices[7], colour);
        DebugRenderer::draw_hair_line(vertices[7], vertices[4], colour);
        DebugRenderer::draw_hair_line(vertices[0], vertices[4], colour);
        DebugRenderer::draw_hair_line(vertices[1], vertices[5], colour);
        DebugRenderer::draw_hair_line(vertices[2], vertices[6], colour);
        DebugRenderer::draw_hair_line(vertices[3], vertices[7], colour);
    }
    void DebugRenderer::debug_draw(const RectLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        // Draw rectangle representation
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();

        glm::vec3 half_size(light->width * 0.5f, light->height * 0.5f, 0.0f);
        glm::vec3 vertices[8] = {
            half_size,
            {-half_size.x, half_size.y, 0.0f},
            {-half_size.x, -half_size.y, 0.0f},
            {half_size.x, -half_size.y, 0.0f},
            {half_size.x, half_size.y, 0.01f},
            {-half_size.x, half_size.y, 0.01f},
            {-half_size.x, -half_size.y, 0.01f},
            {half_size.x, -half_size.y, 0.01f}
        };

        for (auto& v : vertices)
            v = position + (rotation * v);

        DebugRenderer::draw_hair_line(vertices[0], vertices[1], colour);
        DebugRenderer::draw_hair_line(vertices[1], vertices[2], colour);
        DebugRenderer::draw_hair_line(vertices[2], vertices[3], colour);
        DebugRenderer::draw_hair_line(vertices[3], vertices[0], colour);
        DebugRenderer::draw_hair_line(vertices[4], vertices[5], colour);
        DebugRenderer::draw_hair_line(vertices[5], vertices[6], colour);
        DebugRenderer::draw_hair_line(vertices[6], vertices[7], colour);
        DebugRenderer::draw_hair_line(vertices[7], vertices[4], colour);
        DebugRenderer::draw_hair_line(vertices[0], vertices[4], colour);
        DebugRenderer::draw_hair_line(vertices[1], vertices[5], colour);
        DebugRenderer::draw_hair_line(vertices[2], vertices[6], colour);
        DebugRenderer::draw_hair_line(vertices[3], vertices[7], colour);
    }

    void DebugRenderer::debug_draw(const SpotLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        auto angle = light->outer_angle;
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();
        debug_draw_cone(20, 4, angle, common->intensity, position, rotation, colour);
    }

    void DebugRenderer::debug_draw(const DirectionalLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();
        auto direction = transform.get_forward_direction();
        glm::vec3 offset(0.0f, 0.1f, 0.0f);
        draw_hair_line(position + offset, position + direction * 2.0f + offset, colour);
        draw_hair_line(position - offset, position + direction * 2.0f - offset, colour);

        draw_hair_line(position, position + direction * 2.0f, colour);
        debug_draw_cone(20, 4, 30.0f, 1.5f, position - direction * 1.5f, rotation, colour);
    }

    void DebugRenderer::debug_draw(const PointLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();
        debug_draw_sphere(light->radius, position, colour);
    }

    void DebugRenderer::debug_draw(const DiskLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();
        debug_draw_circle(30, light->radius, position, rotation, colour);
    }

    void DebugRenderer::debug_draw(const CylinderLight* light, const maths::Transform& transform, const LightCommon* common, const glm::vec4& colour)
    {
        auto position = transform.get_world_position();
        auto rotation = transform.get_world_orientation();
        debug_draw_capsule(position, rotation, light->length, light->radius, colour);
    }

    void DebugRenderer::debug_draw_circle(int numVerts, float radius, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        float step = 360.0f / float(numVerts);

        for(int i = 0; i < numVerts; i++)
        {
            float cx          = maths::Cos(step * i) * radius;
            float cy          = maths::Sin(step * i) * radius;
            glm::vec3 current = glm::vec3(cx, cy, 0.0f);

            float nx       = maths::Cos(step * (i + 1)) * radius;
            float ny       = maths::Sin(step * (i + 1)) * radius;
            glm::vec3 next = glm::vec3(nx, ny, 0.0f);

            draw_hair_line(position + (rotation * current), position + (rotation * next), colour);
        }
    }
    void DebugRenderer::debug_draw_sphere(float radius, const glm::vec3& position, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        float offset = 0.0f;
        debug_draw_circle(20, radius, position, glm::quat(glm::vec3(0.0f, 0.0f, 0.0f)), colour);
        debug_draw_circle(20, radius, position, glm::quat(glm::vec3(90.0f, 0.0f, 0.0f)), colour);
        debug_draw_circle(20, radius, position, glm::quat(glm::vec3(0.0f, 90.0f, 90.0f)), colour);
    }

    void DebugRenderer::debug_draw_cone(int numCircleVerts, int numLinesToCircle, float angle, float length, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        float endAngle        = maths::Tan(angle * 0.5f) * length;
        glm::vec3 forward     = -(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 endPosition = position + forward * length;
        float offset          = 0.0f;
        debug_draw_circle(numCircleVerts, endAngle, endPosition, rotation, colour);

        for(int i = 0; i < numLinesToCircle; i++)
        {
            float a         = i * 90.0f;
            glm::vec3 point = rotation * glm::vec3(maths::Cos(a), maths::Sin(a), 0.0f) * endAngle;
            draw_hair_line(position, position + point + forward * length, colour);
        }
    }

    void debug_drawArc(int numVerts, float radius, const glm::vec3& start, const glm::vec3& end, const glm::quat& rotation, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        float step    = 180.0f / numVerts;
        glm::quat rot = glm::lookAt(rotation * start, rotation * end, glm::vec3(0.0f, 1.0f, 0.0f));
        rot           = rotation * rot;

        glm::vec3 arcCentre = (start + end) * 0.5f;
        for(int i = 0; i < numVerts; i++)
        {
            float cx          = maths::Cos(step * i) * radius;
            float cy          = maths::Sin(step * i) * radius;
            glm::vec3 current = glm::vec3(cx, cy, 0.0f);

            float nx       = maths::Cos(step * (i + 1)) * radius;
            float ny       = maths::Sin(step * (i + 1)) * radius;
            glm::vec3 next = glm::vec3(nx, ny, 0.0f);

            DebugRenderer::draw_hair_line(arcCentre + (rot * current), arcCentre + (rot * next), colour);
        }
    }

    void DebugRenderer::debug_draw_capsule(const glm::vec3& position, const glm::quat& rotation, float height, float radius, const glm::vec4& colour)
    {
        DS_PROFILE_FUNCTION();
        glm::vec3 up = (rotation * glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 topSphereCentre    = position + up * (height * 0.5f);
        glm::vec3 bottomSphereCentre = position - up * (height * 0.5f);

        debug_draw_circle(20, radius, topSphereCentre, rotation * glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)), colour);
        debug_draw_circle(20, radius, bottomSphereCentre, rotation * glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)), colour);

        // Draw 10 arcs
        // Sides
        float step = 360.0f / float(20);
        for(int i = 0; i < 20; i++)
        {
            float z = maths::Cos(step * i) * radius;
            float x = maths::Sin(step * i) * radius;

            glm::vec3 offset = rotation * glm::vec4(x, 0.0f, z, 0.0f);
            draw_hair_line(bottomSphereCentre + offset, topSphereCentre + offset, colour);

            if(i < 10)
            {
                float z2 = maths::Cos(step * (i + 10)) * radius;
                float x2 = maths::Sin(step * (i + 10)) * radius;

                glm::vec3 offset2 = rotation * glm::vec4(x2, 0.0f, z2, 0.0f);
                // Top Hemishpere
                debug_drawArc(20, radius, topSphereCentre + offset, topSphereCentre + offset2, rotation, colour);
                // Bottom Hemisphere
                debug_drawArc(20, radius, bottomSphereCentre + offset, bottomSphereCentre + offset2, rotation * glm::quat(glm::vec3(glm::radians(180.0f), 0.0f, 0.0f)), colour);
            }
        }
    }

    void DebugRenderer::debug_draw(const maths::Ray& ray, const glm::vec4& colour, float distance)
    {
        DS_PROFILE_FUNCTION();
        draw_hair_line(ray.Origin, ray.Origin + ray.Direction * distance, colour);
    }
}
