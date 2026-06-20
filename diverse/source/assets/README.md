# Asset Management System Architecture

## Overview

Divshot 的资源管理系统已重构为四层架构，实现了 CPU 和 GPU 资源的完全分离，支持完整的运行时热重载和预算控制的 GPU 内存管理。

## 四层架构

### Layer 1: AssetDB / AssetRegistry
**文件**: `asset_registry.h`, `asset_metadata.h`

- **职责**: 资产 ID、路径、类型、导入参数、依赖图、版本、热重载
- **功能**:
  - 资产元数据管理
  - 依赖关系跟踪（正向和反向）
  - 循环依赖检测
  - 版本跟踪（用于热重载）
  - 路径到 ID 的映射

**使用示例**:
```cpp
auto& registry = AssetRegistry::get_instance();

// 注册资产
AssetId id = GenerateAssetId();
AssetMetadata metadata;
metadata.id = id;
metadata.source_path = "textures/brick.png";
metadata.type = AssetType::Texture;
registry.register_asset(id, metadata);

// 添加依赖
registry.add_dependency(material_id, texture_id);

// 查询资产
auto metadata = registry.get_metadata(id);
auto dependents = registry.get_dependents(id);  // 获取依赖于该资产的资产
```

### Layer 2: AssetCache / Loader
**文件**: `asset_cache.h`, `asset_loader.h`

- **职责**: 异步 IO、解码、CPU 数据缓存、状态机、失败回退
- **功能**:
  - 异步加载（线程池）
  - LRU 缓存管理
  - 延迟释放（GPU 上传后可释放 CPU 数据）
  - 加载状态机：Unloaded → Loading → Loaded → GpuUploaded → Evictable
  - 失败回退到默认资产

**使用示例**:
```cpp
// 创建缓存
AssetCache<TextureAsset> texture_cache;

// 插入资产
auto texture = std::make_shared<TextureAsset>();
texture_cache.insert(asset_id, texture);

// 标记为已上传到 GPU（可释放 CPU 数据）
texture_cache.mark_gpu_uploaded(asset_id);

// 内存压力时驱逐
size_t target_bytes = 50 * 1024 * 1024;  // 50MB
size_t freed = texture_cache.evict_lru(target_bytes);
```

### Layer 3: GpuResourceSystem
**文件**: `rendering/gpu_resource_system.h`, `rendering/gpu_resource_system.cpp`

- **职责**: 上传队列、staging、GPU residency、bindless index、延迟释放
- **功能**:
  - 优先级上传队列（Critical > High > Normal > Low）
  - GPU 内存预算控制（高/低水位线）
  - Bindless 描述符管理
  - 帧安全的延迟释放
  - Staging 缓冲池

**使用示例**:
```cpp
// 初始化
GpuResourceSystem gpu_sys(device);
gpu_sys.set_budget_config({512, 256, 0.9f, 0.7f});  // 512MB textures, 256MB buffers

// 上传资产
gpu_sys.queue_upload(asset_id, AssetType::Texture, UploadPriority::High);

// 获取 bindless 槽位
BindlessImageHandle handle = gpu_sys.allocate_bindless_slot(texture_ptr);
gpu_sys.update_bindless_descriptor(handle, texture_ptr);

// 延迟释放
gpu_sys.defer_release(resource, current_frame + MAX_FRAMES_IN_FLIGHT, asset_id);
gpu_sys.process_deferred_releases(completed_frame);
```

### Layer 4: RenderGraph
**文件**: 现有的 `drs_rg/renderer.h`

- **职责**: 只管 frame-local/transient 资源、barrier、aliasing、pass 依赖
- **说明**: 现有系统已正确处理，不需要修改

## 核心数据类型

### CPU-Only 资产

#### TextureAsset
```cpp
struct TextureAsset {
    AssetId id;
    std::filesystem::path source_path;
    TextureImportSettings settings;
    std::vector<MipData> mips;
    PixelFormat format;
    std::array<u32, 3> extent;
    uint32_t version;
    size_t cpu_memory_size;
    // 无 GPU 状态！
};
```

#### MeshAsset
```cpp
struct MeshAsset {
    AssetId id;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    maths::BoundingBox bounding_box;
    uint32_t version;
    size_t cpu_memory_size;
    // 无 GPU 缓冲区！
};
```

### GPU 资源结构

#### TextureGpu
```cpp
struct TextureGpu {
    std::shared_ptr<rhi::GpuTexture> texture;
    BindlessImageHandle srv;
    uint32_t resident_version;
    size_t gpu_memory_size;
};
```

#### MeshGpu
```cpp
struct MeshGpu {
    GpuBufferHandle vertex_buffer;
    GpuBufferHandle index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t resident_version;
};
```

### 资产句柄

#### AssetHandle
```cpp
template<typename AssetType>
class AssetHandle {
    AssetId id;
    uint32_t generation;  // 版本验证
};
```

## Material 系统重构

### MaterialAsset（CPU）
```cpp
struct MaterialAsset {
    AssetId id;
    MaterialProperties properties;

    // 使用 AssetHandle，而非 SharedPtr！
    AssetHandle<TextureAsset> albedo;
    AssetHandle<TextureAsset> normal;
    AssetHandle<TextureAsset> metallic;
    AssetHandle<TextureAsset> roughness;
    AssetHandle<TextureAsset> ao;
    AssetHandle<TextureAsset> emissive;
};
```

### MaterialGpu（GPU）
```cpp
struct MaterialGpu {
    uint32_t material_buffer_index;
    uint32_t resident_version;
    std::array<uint32_t, 8> texture_bindless_indices;
};
```

## 热重载支持

### 文件观察器
```cpp
AssetFileWatcher watcher;

// 监视文件
watcher.watch_file("textures/brick.png", [](const FileChangeEvent& event) {
    if (event.type == FileChangeType::Modified) {
        // 触发资产重载
        AssetRegistry::get_instance().increment_version(asset_id);
    }
});

// 每帧更新
watcher.update();
```

### 热重载流程
```
文件变化检测
    ↓
AssetRegistry.increment_version(asset_id)
    ↓
通知所有依赖者
    ↓
AssetCache.unload(old_version)
    ↓
AssetLoader.reload(new_version)
    ↓
GpuResourceSystem.reload_asset()
    ↓
GPU 描述符原子更新
```

## 使用指南

### 初始化系统
```cpp
#include "assets/asset_system.h"

// 初始化
AssetSystem::get_instance().initialize(device);

// 每帧更新
AssetSystem::get_instance().update(frame_index, delta_time);

// 关闭
AssetSystem::get_instance().shutdown();
```

### 加载资产
```cpp
auto& asset_sys = AssetSystem::get_instance();

// 加载纹理
auto texture = asset_sys.load_asset<TextureAsset>("textures/brick.png");

// 队列 GPU 上传
asset_sys.queue_gpu_upload(texture->id, UploadPriority::Normal);
```

### 访问 GPU 资源
```cpp
auto& gpu_sys = AssetSystem::get_instance().gpu_system();

// 获取纹理 GPU 资源
auto texture_gpu = gpu_sys.get_texture_gpu(asset_id);
if (texture_gpu.texture) {
    // 使用纹理
}

// 分配 bindless 槽位
BindlessImageHandle handle = gpu_sys.allocate_bindless_slot(texture_gpu.texture.get());
```

## 迁移指南

### 从旧系统迁移

1. **Texture 类**: 移除 `gpu_texture` 成员，使用 `TextureAsset + GpuResourceSystem`
2. **Mesh 类**: 移除 `vertex_buffer/index_buffer`，使用 `MeshAsset + GpuResourceSystem`
3. **Material 类**: 替换 `SharedPtr<Texture>` 为 `AssetHandle<TextureAsset>`
4. **渲染器集成**: 使用 `GpuResourceSystem` 替代直接的 GPU 资源创建

### 兼容性

这是一个**完全重写**的实现，不保持向后兼容性。现有代码需要适配新架构。

## 文件列表

### 新创建的文件

**核心类型**:
- `assets/asset_id.h` - 资产 ID
- `assets/asset_handle.h` - 资产句柄
- `assets/asset_metadata.h` - 资产元数据

**CPU 资产**:
- `assets/cpu_assets.h` - TextureAsset, MeshAsset
- `assets/cpu_assets.cpp`

**GPU 资产**:
- `assets/gpu_assets.h` - TextureGpu, MeshGpu, MaterialGpu

**Layer 1 - AssetDB**:
- `assets/asset_registry.h`
- `assets/asset_registry.cpp`

**Layer 2 - Cache/Loader**:
- `assets/asset_cache.h`
- `assets/asset_loader.h`
- `assets/asset_loader.cpp`

**Layer 3 - GPU System**:
- `rendering/gpu_resource_system.h`
- `rendering/gpu_resource_system.cpp`

**Material**:
- `assets/material_asset.h`
- `assets/material_asset.cpp`

**热重载**:
- `assets/asset_file_watcher.h`
- `assets/asset_file_watcher.cpp`

**整合**:
- `assets/asset_system.h`
- `assets/asset_system.cpp`

## 下一步工作

1. **集成加载器**: 实现具体的纹理、网格加载函数
2. **渲染器集成**: 更新 `DeferedRenderer` 使用新的 `GpuResourceSystem`
3. **测试**: 编写单元测试和集成测试
4. **文档**: 添加 API 文档和使用示例

## 参考

- 计划文档: `.claude/plans/deep-snuggling-pretzel.md`
- 现有资产系统: `assets/texture.h`, `assets/mesh.h`, `assets/material.h`
- RHI 接口: `backend/drs_rhi/gpu_resource.h`
