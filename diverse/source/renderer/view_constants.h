#pragma once
#include "core/base_type.h"
#include <array>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/matrix_access.hpp>
namespace diverse
{
    struct CameraMatrices
    {
        glm::mat4 view_to_clip;
        glm::mat4 clip_to_view;
        glm::mat4 world_to_view;
        glm::mat4 view_to_world;

        auto eye_position() -> glm::vec3
        {
            return (view_to_world * glm::vec4(0.0, 0.0, 0.0, 1.0));
        }

        auto eye_direction() -> glm::vec3
        {
            auto vec = glm::vec3(view_to_world * glm::vec4(0.0, 0.0, -1.0, 0.0));
            return glm::normalize(vec);
        }

        auto aspect_ratio() -> f32
        {
            return view_to_clip[1].y / view_to_clip[0].x;
        }
    };

    struct ViewConstants
    {
        glm::mat4 view_to_clip;
        glm::mat4 clip_to_view;
        glm::mat4 view_to_sample;
        glm::mat4 sample_to_view;
        glm::mat4 world_to_view;
        glm::mat4 view_to_world;
        glm::mat4 world_to_clip;

        glm::mat4 clip_to_prev_clip;

        glm::mat4 prev_view_to_prev_clip;
        glm::mat4 prev_clip_to_prev_view;
        glm::mat4 prev_world_to_prev_view;
        glm::mat4 prev_view_to_prev_world;

        glm::vec2 sample_offset_pixels;
        glm::vec2 sample_offset_clip;
        glm::vec4 viewport_size;
        ViewConstants() {}
        ViewConstants(const CameraMatrices& camera_matrices,
            const CameraMatrices& prev_camera_matrices,
            const std::array<u32, 2>& render_extent)
        {
            clip_to_prev_clip = prev_camera_matrices.view_to_clip
                * prev_camera_matrices.world_to_view
                * camera_matrices.view_to_world
                * camera_matrices.clip_to_view;

            view_to_clip = camera_matrices.view_to_clip;
            clip_to_view = camera_matrices.clip_to_view;
            view_to_sample = glm::mat4();
            sample_to_view = glm::mat4();
            world_to_view = camera_matrices.world_to_view;
            view_to_world = camera_matrices.view_to_world;
            prev_view_to_prev_clip = prev_camera_matrices.view_to_clip;
            prev_clip_to_prev_view = prev_camera_matrices.clip_to_view;
            prev_world_to_prev_view = prev_camera_matrices.world_to_view;
            prev_view_to_prev_world = prev_camera_matrices.view_to_world;
            sample_offset_pixels = glm::vec2(0.0f);
            sample_offset_clip = glm::vec2(0.0f);
            world_to_clip = view_to_clip * world_to_view;
            viewport_size = glm::vec4(
                render_extent[0],
                render_extent[1],
                1.0f / render_extent[0],
                1.0f / render_extent[1]
            );
            set_pixel_offset(sample_offset_pixels, render_extent);
        }

        auto eye_position() -> glm::vec3
        {
            auto eye_pos_h = view_to_world[3];
            return glm::vec3(eye_pos_h.x / eye_pos_h.w, eye_pos_h.y / eye_pos_h.w, eye_pos_h.z / eye_pos_h.w);
        }

        auto prev_eye_position() -> glm::vec3
        {
            auto eye_pos_h = prev_view_to_prev_world[3];
            return glm::vec3(eye_pos_h.x / eye_pos_h.w, eye_pos_h.y / eye_pos_h.w, eye_pos_h.z / eye_pos_h.w);
        }
        auto set_pixel_offset(const glm::vec2& v, const std::array<u32, 2>& render_extent) -> void
        {
            auto sample_pixels = v;
            auto sample_clip = glm::vec2(
                (2.0f * v.x) / render_extent[0],
                (2.0f * v.y) / render_extent[1]
            );

            auto  jitter_matrix = glm::identity<glm::mat4>();
            jitter_matrix[3] = glm::vec4(-sample_clip, 0.0f, 1.0f);

            auto jitter_matrix_inv = glm::identity<glm::mat4>();
            jitter_matrix_inv[3] = glm::vec4(sample_clip, 0.0f, 1.0f);

            auto view_2_sample = jitter_matrix * this->view_to_clip;
            auto sample_2_view = this->clip_to_view * jitter_matrix_inv;

            this->view_to_sample = view_2_sample;
            this->sample_to_view = sample_2_view;
            this->sample_offset_pixels = sample_pixels;
            this->sample_offset_clip = sample_clip;
        }

        auto transpose()->ViewConstants
        {
            ViewConstants result;
            result.view_to_clip = glm::transpose(view_to_clip);
            result.clip_to_view = glm::transpose(clip_to_view);
            result.view_to_sample = glm::transpose(view_to_sample);
            result.sample_to_view = glm::transpose(sample_to_view);
            result.world_to_view = glm::transpose(world_to_view);
            result.view_to_world = glm::transpose(view_to_world);
            result.world_to_clip = glm::transpose(world_to_clip);
            result.clip_to_prev_clip = glm::transpose(clip_to_prev_clip);
            result.prev_view_to_prev_clip = glm::transpose(prev_view_to_prev_clip);
            result.prev_clip_to_prev_view = glm::transpose(prev_clip_to_prev_view);
            result.prev_world_to_prev_view = glm::transpose(prev_world_to_prev_view);
            result.prev_view_to_prev_world = glm::transpose(prev_view_to_prev_world);
            result.sample_offset_pixels = sample_offset_pixels;
            result.sample_offset_clip = sample_offset_clip;
            result.viewport_size = viewport_size;
            return result;
        }
    };
}