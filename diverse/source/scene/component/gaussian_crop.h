#pragma once

#include <maths/transform.h>
#include <maths/bounding_box.h>
#include <maths/bounding_sphere.h>
#include <maths/rect.h>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <deque>

namespace diverse
{
    class GaussianCrop
    {
    public:
        GaussianCrop();
        ~GaussianCrop();
        enum class CropType
        {
            Box,
            Sphere,
            Plane
        };
        struct GaussianCropData
        {
            GaussianCropData();
            maths::BoundingSphere   bdsphere;
            maths::BoundingBox      bdbox;
            maths::Transform        transform;
            int crop_op = 0x0000FFFF;

            void set_crop_type(CropType type);
            CropType get_crop_type() const;
            void set_crop_state(u32 v);
            u32  get_crop_state() const;
            maths::BoundingBox& bouding_box() { return bdbox; }
            maths::BoundingSphere& bouding_sphere() { return bdsphere; }
            const maths::BoundingBox& bouding_box() const { return bdbox; }
            const maths::BoundingSphere& bouding_sphere() const{ return bdsphere; }
        };

        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("crop_cnt", (int)crop_datas.size()));
            for (auto i = 0; i < crop_datas.size(); i++)
            { 
                auto& crop_data = crop_datas[i];
                auto crop_type = crop_data.get_crop_type();
                archive(cereal::make_nvp("crop_type", (int)crop_type));
                archive(cereal::make_nvp("radius", crop_data.bdsphere.get_radius()), cereal::make_nvp("center", crop_data.bdsphere.get_center()));
                archive(cereal::make_nvp("pmin", crop_data.bdbox.min()), cereal::make_nvp("pmax", crop_data.bdbox.max()));
                archive(cereal::make_nvp("transform", crop_data.transform));
            }
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            int cnt;
            archive(cereal::make_nvp("crop_cnt", cnt));
            crop_datas.resize(cnt);
            for(auto i=0;i<cnt;i++)
            { 
                auto& crop_data = crop_datas[i];
                int crop_type;
                float radius;glm::vec3 center;
                glm::vec3 pmin,pmax;
                archive(cereal::make_nvp("crop_type", crop_type));
                archive(cereal::make_nvp("radius", radius), cereal::make_nvp("center", center));
                archive(cereal::make_nvp("pmin", pmin), cereal::make_nvp("pmax", pmax));
                archive(cereal::make_nvp("transform", crop_data.transform));
                crop_data.set_crop_type((CropType)crop_type);
                crop_data.bdsphere = maths::BoundingSphere(center, radius);
                crop_data.bdbox = maths::BoundingBox(pmin, pmax);
            }
        }

        auto get_crop_data() ->std::vector<GaussianCropData>& {return crop_datas;}
        auto get_crop_data() const ->const std::vector<GaussianCropData>& {return crop_datas;}

        void set_init_crop_data(const GaussianCropData& data);
    protected:
        std::vector<GaussianCropData> crop_datas;
    };
}