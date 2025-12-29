#pragma once

namespace diverse
{
    struct Version
    {
        int major = 1;
        int minor = 3;
        int patch = 1;
    };

    constexpr Version const DiverseVersion = Version();
}
