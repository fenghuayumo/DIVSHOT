#pragma once

#include "assets/asset.h"
#include <ozz/base/memory/unique_ptr.h>

namespace ozz
{
    namespace animation
    {
        class Skeleton;
    }
}

namespace diverse
{
    class Skeleton : public Asset
    {
    public:
        Skeleton(const std::string& filename);
        Skeleton(ozz::animation::Skeleton* skeleton);

        virtual ~Skeleton();

        const std::string& get_file_path() const { return m_FilePath; }

        static AssetType get_static_type() { return AssetType::Skeleton; }
        virtual AssetType get_asset_type() const override { return get_static_type(); }

        bool valid() { return m_Skeleton != nullptr; }
        const ozz::animation::Skeleton& get_skeleton() const
        {
            DS_ASSERT(m_Skeleton, "Attempted to access null skeleton!");
            return *m_Skeleton;
        }

    private:
        std::string m_FilePath;
        ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton;
    };
}
