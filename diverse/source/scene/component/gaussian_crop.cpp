#include "gaussian_crop.h"
#include "backend/drs_rhi/gpu_device.h"

namespace diverse
{
    GaussianCrop::GaussianCropData::GaussianCropData()
	{
        //bdbox = maths::BoundingBox(glm::vec3(-1, -1, -1), glm::vec3(1, 1, 1));
        //bdsphere = maths::BoundingSphere(glm::vec3(0, 0, 0), 2.0f);
        //set_crop_type(GaussianCrop::CropType::Box);
	}
    GaussianCrop::GaussianCrop()
    {
  /*      crop_datas.push_back(diverse::GaussianCrop::GaussianCropData());
        crop_datas.back().bdbox = maths::BoundingBox(glm::vec3(-1,-1,-1), glm::vec3(1,1,1));
        crop_datas.back().bdsphere = maths::BoundingSphere(glm::vec3(0,0,0), 2.0f);
        crop_datas.back().set_crop_type(GaussianCrop::CropType::Box);*/
    }

    GaussianCrop::~GaussianCrop()
    {
    }

    void GaussianCrop::set_init_crop_data(const GaussianCropData& data)
    {
        crop_datas.push_back(data);
    }

    void GaussianCrop::GaussianCropData::set_crop_type(CropType type)
    {
        crop_op = (crop_op & 0xFF00FFFF) | (((int)type & 0xFF) << 16);
    }
    //get crop type
    GaussianCrop::CropType GaussianCrop::GaussianCropData::get_crop_type() const
    {
        return static_cast<GaussianCrop::CropType>((crop_op >> 16) & 0xFF);
    }

    void GaussianCrop::GaussianCropData::set_crop_state(u32 v)
    {
        crop_op = (crop_op & 0xF0FFFFFF) | ((v & 0x0F) << 24);
	}

    u32 GaussianCrop::GaussianCropData::get_crop_state() const
    {  
        return (crop_op >> 24) & 0x0F;
    }

}
