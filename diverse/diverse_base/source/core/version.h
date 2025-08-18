#pragma once

namespace diverse
{
    struct Version
    {
        int major = 1;
        int minor = 0;
        int patch = 2;
    };

    constexpr Version const DiverseVersion = Version();
}
