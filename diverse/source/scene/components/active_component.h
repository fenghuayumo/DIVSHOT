#pragma once

#include <cereal/cereal.hpp>

namespace diverse
{

    struct ActiveComponent
    {
        bool active = true;

        ActiveComponent() = default;
        explicit ActiveComponent(bool a) : active(a) {}

        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(cereal::make_nvp("active", active));
        }
    };

}
