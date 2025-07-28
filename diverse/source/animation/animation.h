#pragma once

#include "assets/asset.h"
#include <ozz/base/memory/unique_ptr.h>

namespace ozz
{
    namespace animation
    {
        class Animation;
    }
}

namespace diverse
{
    class Skeleton;
    class DS_EXPORT Animation : public Asset
    {
    public:
        Animation(const std::string& filename, const std::string& animationName, SharedPtr<Skeleton> skeleton);
        Animation(const std::string& animationName, ozz::animation::Animation* animation, SharedPtr<Skeleton> skeleton);

        virtual ~Animation();

        const std::string& get_file_path() const { return m_FilePath; }
        const std::string& get_name() const { return m_AnimationName; }
        const SharedPtr<Skeleton>& GetSkeleton() const { return m_Skeleton; }

        static AssetType get_static_type() { return AssetType::Animation; }
        virtual AssetType get_asset_type() const override { return get_static_type(); }

        const ozz::animation::Animation& get_animation() const
        {
            DS_ASSERT(m_Animation, "Attempted to access null animation!");
            return *m_Animation;
        }

    private:
        SharedPtr<Skeleton> m_Skeleton;
        std::string m_FilePath;
        std::string m_AnimationName;
        ozz::unique_ptr<ozz::animation::Animation> m_Animation;
    };

}
