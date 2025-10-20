#pragma once

namespace diverse
{
    struct Version
    {
        int major = 1;
        int minor = 1;
        int patch = 0;
    };

    constexpr Version const DiverseVersion = Version();
}
