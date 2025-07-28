#include <string>
#include "core/reference.h"
#include "animation.h"
#include "skeleton.h"
#include "core/reference.h"
#include "core/ds_log.h"

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/span.h>

namespace diverse
{
    Animation::Animation(const std::string& filename, const std::string& animationName, SharedPtr<Skeleton> skeleton)
        : m_Skeleton(skeleton)
        , m_FilePath(filename)
        , m_AnimationName(animationName)
    {

        DS_LOG_INFO("Loading animation: %s", m_FilePath.c_str());

        if(!skeleton.get() || !skeleton->is_valid())
        {
            DS_LOG_ERROR("Invalid skeleton passed to animation asset for file '%s'", m_FilePath.c_str());
            set_flag(AssetFlag::Invalid);
            return;
        }
    }

    Animation::Animation(const std::string& animationName, ozz::animation::Animation* animation, SharedPtr<Skeleton> skeleton)
        : m_Skeleton(skeleton)
        , m_AnimationName(animationName)
        , m_Animation(animation)
    {
        DS_LOG_INFO("Loading animation: %s", animationName.c_str());
    }

    Animation::~Animation()
    {
    }
}
