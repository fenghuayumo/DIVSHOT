#pragma once
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include "maths/maths_utils.h"
#include <optional>

 #define SUN_CONTROLLER_SQUISH 0.2
 namespace diverse
 {
     struct SunController
     {
         SunController()
         {
             set_towards_sun(glm::vec3(0.44990778, 0.47876653, -0.75390023));
             latent = calculate_latent(sun_dir);
         }
         auto towards_sun()->glm::vec3
         {
             return sun_dir;
         }

         auto set_towards_sun(const glm::vec3& dir)->void
         {
             this->sun_dir = dir;
             this->latent = {};
         }

         auto calculate_towards_sun(const glm::vec2& latent)->glm::vec3
         {
             auto xz = latent;
             auto len = glm::length(xz);
             f32 ysgn = 1.0f;
             if( len > 1.0 )
             {
                 xz *= (2.0f - len) / len;
                 ysgn = -1.0f;
             }else
                 ysgn = 1.0f;
            
             auto y =  SUN_CONTROLLER_SQUISH * ysgn * (1.0f - glm::length2(xz));
             return glm::normalize(glm::vec3(xz.x,y,xz.y));
         }

         auto calculate_towards_sun_from_theta_phi(float theta,float phi) -> glm::vec3
         {
             auto x = std::cos(theta) * std::cos(phi);
			 auto y = std::sin(theta);
			 auto z = std::cos(theta) * std::sin(phi);
             sun_dir = glm::normalize(glm::vec3(x,y,z));
             latent = calculate_latent(sun_dir);
             return sun_dir;
         }

         auto calculate_latent(const glm::vec3& sun_dir) -> glm::vec2 
         {
             auto t = SUN_CONTROLLER_SQUISH;
             auto t2 = t * t;

             auto y2 = sun_dir.y * sun_dir.y;
             auto y4 = y2 * y2;

             auto a = -y2 + 2.0 * t2 * (-1.0 + y2) + std::sqrt(y4 - 4.0 * t2 * y2 * (-1.0 + y2));
             auto b = 2.0 * t2 * (-1.0 + y2);
             auto xz_len = std::sqrtf(a / b);

             if( !isfinite(xz_len) && !isnan(xz_len))
                 xz_len = 0.0f;
            
             auto xz = sun_dir.xz() * (xz_len / std::max<f32>(1e-10, glm::length(sun_dir.xz())) );

             if( sun_dir.y < 0.0 ) 
                 xz *= (2.0 - xz_len) / xz_len;
             return xz;
         }

         auto view_space_rotate(const glm::quat& ref_frame, f32 delta_x,f32 delta_y)->void
         {
             glm::vec2 xz = latent.value_or(calculate_latent(sun_dir));

             const f32 MOVE_SPEED = 0.2f;

             auto xz_norm = glm::normalize(xz);
             auto rotation_strength = maths::smoothstep(1.2f, 1.5f, glm::length(xz));
             auto delta = ref_frame * glm::vec3(-delta_x, 0.0f, -delta_y);
             auto move_align = glm::determinant(glm::mat2(delta.xz(), xz_norm));
             
             xz += (delta * MOVE_SPEED).xz();
             auto rm = glm::toMat3(glm::angleAxis(rotation_strength, glm::vec3(1,0,0)));
             xz = rm * glm::vec3(xz, 0.0f);
             {
                 auto len = glm::length(xz);
                 if( len > 2.0f )
                 {
                     xz *= -(3.0 -len )/ len;
                 }
             }

             latent = xz;
             sun_dir = calculate_towards_sun(xz);
         }

         inline auto operator==(const SunController& other)->bool
         {
            return sun_dir == other.sun_dir;
         }
         std::optional<glm::vec2>    latent;
     protected:
         glm::vec3     sun_dir = glm::vec3(0,1,0);
     };
 }