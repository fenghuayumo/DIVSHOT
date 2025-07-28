#pragma once

#include "engine/file_system.h"
#include "scene/serialisation.h"

namespace diverse
{
  
    struct PrefabComponent
    {
        PrefabComponent(const std::string& path)
        {
            Path = path;
        }

        std::string Path;
        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(Path);
        }
    };
}
