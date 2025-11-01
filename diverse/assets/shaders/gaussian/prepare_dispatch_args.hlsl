[[vk::binding(0)]] ByteAddressBuffer num_visible_buffer;
[[vk::binding(1)]] RWByteAddressBuffer dispatch_args;

[numthreads(1, 1, 1)]
void main() {
    // Aging ags
    {
        const uint entry_count = num_visible_buffer.Load(0);

        // static const uint threads_per_group = 64;
        // static const uint entries_per_thread = 1;
        // static const uint divisor = threads_per_group * entries_per_thread;

        dispatch_args.Store4(0 * sizeof(uint4), uint4(4, entry_count, 0, 0));
    }
}