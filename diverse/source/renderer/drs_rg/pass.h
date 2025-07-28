#pragma once
#include "backend/drs_rhi/gpu_resource.h"
#include "resource.h"
#include <functional>
#include <vector>


namespace diverse
{
    namespace rg
    {
        enum PassResourceAccessSyncType
        {
            AlwaysSync,
            SkipSyncIfSameAccessType
        };

        struct PassResourceAccessType
        {
            rhi::AccessType access_type;
            PassResourceAccessSyncType  sync_type;
        };
        struct PassResourceRef
        {
            GraphRawResourceHandle handle;
            PassResourceAccessType access;
        };

        struct RecordedPass
        {
            RecordedPass(const std::string& p_name, uint32 p_idx)
            :name(p_name), idx(p_idx)
            {}
            RecordedPass(){}
            RecordedPass(const RecordedPass& other) = delete;
            RecordedPass(RecordedPass&& other) noexcept
            {
                *this = std::move(other);
            }
            RecordedPass& operator=(RecordedPass&& other) noexcept 
            {
                std::swap(read, other.read);
                std::swap(write, other.write);
                std::swap(name, other.name);
                std::swap(idx, other.idx);
                std::swap(render_fn, other.render_fn);
                return *this;
            }
            std::vector<PassResourceRef> read;
            std::vector<PassResourceRef> write;
            std::string name;

            std::function<void(struct RenderPassApi& api)> render_fn;
            uint32 idx;
        };
    }
}