
## diverse
<p align="center">
The core rendering lib which implement gsplat rendering and mesh rendering(include raster and ray traing pipeline)
</p>

### Extend RenderPass
   it will be very simple to create a new pass, just create resource and bind corresponding location using rendergraph. there is a simple example:
```cpp
    auto main_img = rg.create<rhi::GpuTexture>(desc)
    auto ui_img = rg.create<rhi::GpuTexture>(desc)
    auto buf = rg.create<rhi::GpuBuffer>(desc)
    rg::RenderPass::new_compute(
        rg.add_pass("final blit"),
        "/shaders/final_blit.hlsl")
        .read(main_img)
        .read(ui_img)
        .write(swap_chain)
        .constants(std::tuple{
            main_img.desc.extent_inv_extent_2d(),
            std::array<float,4>{ (float)swapchain_extent[0],
                    (float)swapchain_extent[1],
                    1.0f / swapchain_extent[0],
                    1.0f / swapchain_extent[1]}
            })
        .dispatch({ swapchain_extent[0], swapchain_extent[1], 1 });
```