#pragma once

#include <string>
#include <cereal/cereal.hpp>

namespace diverse
{

    struct NameComponent
    {
        std::string name;

        NameComponent() = default;
        explicit NameComponent(const std::string& n) : name(n) {}

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(cereal::make_nvp("name", name));
        }
    };

}
