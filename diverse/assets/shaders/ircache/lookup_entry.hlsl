#ifndef IRCACHE_LOOKUP_ENTRY_HLSL
#define IRCACHE_LOOKUP_ENTRY_HLSL

#include "lookup.hlsl"

uint lookup_entry_id(IrcacheLookupParams lookup_param, inout uint rng) {
    IrcacheLookupMaybeAllocate lookup = lookup_param.lookup_maybe_allocate(rng);

    if (lookup.just_allocated) {
        return 0;
    }

    uint entry_idx = 0;
    [unroll]
    for (uint i = 0; i < IRCACHE_LOOKUP_MAX; ++i) if (i < lookup.lookup.count) {
            entry_idx = lookup.lookup.entry_idx[i];
            break;
    }

    return entry_idx;
}

#endif