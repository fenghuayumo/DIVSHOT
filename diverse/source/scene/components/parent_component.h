#pragma once

#include <entt/entity/registry.hpp>
#include <cereal/cereal.hpp>

namespace diverse
{

    struct Parent
    {
        entt::entity value;

        explicit Parent(entt::entity entity = entt::null)
            : value(entity)
        {
        }

        operator entt::entity() const { return value; }
        operator bool() const { return value != entt::null; }

        // Serialization support
        template <typename Archive>
        void serialize(Archive& archive)
        {
            archive(
                cereal::make_nvp("parent", value)
            );
        }
    };

}
