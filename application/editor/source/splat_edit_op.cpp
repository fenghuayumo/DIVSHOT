#include "splat_edit_op.h"
#include "pivot.h"
#include <utility/thread_pool.h>

namespace diverse
{
    auto build_index(GaussianModel* splat, std::function<bool(int)> pred) -> std::vector<u32>
    {
        auto numSplats = 0;
        for (auto i = 0; i < splat->position().size(); i++) 
            if (pred(i)) numSplats++;

        std::vector<u32> result(numSplats);
        auto idx = 0;
        for (auto i = 0; i < splat->position().size(); i++) 
            if (pred(i)) 
                result[idx++] = i;

        return result;
    };

    SplateStateOp::SplateStateOp(GaussianComponent* splat, FilterFunc pred, DoFunc dofunc, UndoFunc undoFunc)
        : SplatEditOperation(splat), indices(build_index(splat->ModelRef.get(), pred)), doIt(dofunc), undoIt(undoFunc)
    {
        // splat->ModelRef->download_state_buffer();
    }

    void SplateStateOp::apply()
    {
        auto model = splat->ModelRef.get();
        parallel_for<size_t>(0, indices.size(), [&](size_t i){
            auto idx = indices[i];
            auto& gs_state = model->state()[idx];
            model->state()[idx] = doIt(gs_state);
        });   
       
        model->update_state();
    }

    void SplateStateOp::undo()
    {
        auto model = splat->ModelRef.get();
        parallel_for<size_t>(0, indices.size(), [&](size_t i){
            auto idx = indices[i];
            auto& gs_state = model->state()[idx];
            model->state()[idx] = undoIt(gs_state);
        });
        model->update_state();
    }

    SplatSelectAllOp::SplatSelectAllOp(GaussianComponent* splat)
        : SplateStateOp(splat, 
                    [splat](int i) { return splat->ModelRef->state()[i] == 0; }, 
                    [](u8 state) { return state | SELECT_STATE; },
                    [](u8 state) { return state & ~SELECT_STATE; })
    {
    }

    SplatSelectInverseOp::SplatSelectInverseOp(GaussianComponent* splat)
        : SplateStateOp(splat, 
                    [splat](int i) { return (splat->ModelRef->state()[i] & (HIDE_STATE | DELETE_STATE)) == 0; }, 
                    [](u8 state) { return state ^ SELECT_STATE; },
                    [](u8 state) { return state ^ SELECT_STATE; })
    {
    }

    SplatSelectNoneOp::SplatSelectNoneOp(GaussianComponent* splat)
        : SplateStateOp(splat, 
                    [splat](int i) { return splat->ModelRef->state()[i] == SELECT_STATE; }, 
                    [](u8 state) { return state & ~SELECT_STATE; },
                    [](u8 state) { return state | SELECT_STATE; })
    {
    }

    HidenSplatOp::HidenSplatOp(GaussianComponent* splat)
        : SplateStateOp(splat, 
                    [splat](int i) { return splat->ModelRef->state()[i] == SELECT_STATE; }, 
                    [](u8 state) { return state | HIDE_STATE; },
                    [](u8 state) { return state & (~HIDE_STATE); })
    {
    }

    UnHidenSplatOp::UnHidenSplatOp(GaussianComponent* splat)
        : SplateStateOp(splat, 
                    [splat](int i) { return (splat->ModelRef->state()[i] & (HIDE_STATE | DELETE_STATE)) == HIDE_STATE; }, 
                    [](u8 state) { return state & (~HIDE_STATE); },
                    [](u8 state) { return state | HIDE_STATE; })
    {
    }

    DeleteSplatEditOp::DeleteSplatEditOp(GaussianComponent* splat)
        : SplateStateOp(splat,
                    [splat](int i) { return splat->ModelRef->state()[i] == SELECT_STATE; },
                    [](u8 state) { return state | DELETE_STATE; },
                    [](u8 state) { return state & (~DELETE_STATE); })
    {
    }

    ResetSplatEditOp::ResetSplatEditOp(GaussianComponent* splat)
        : SplateStateOp(splat,
                    [splat](int i) { return (splat->ModelRef->state()[i] & DELETE_STATE) != 0; },
                    [](u8 state) { return state & (~DELETE_STATE); },
                    [](u8 state) { return state | DELETE_STATE; })
    {
    }

    FilterFunc build_filter_func(GaussianComponent* splat, EditSelectOpType op,FilterFunc filter)
    {
        switch (op)
        {
        case EditSelectOpType::Add:
            return [splat,filter](int i) { return (splat->ModelRef->state()[i] == 0) && filter(i); };
        case EditSelectOpType::Remove:
            return [splat,filter](int i) { return (splat->ModelRef->state()[i] == SELECT_STATE) && filter(i); };
        case EditSelectOpType::Set:
            return [splat,filter](int i) { return (splat->ModelRef->state()[i] == SELECT_STATE) != filter(i);};
        default:
            return [splat,filter](int i) { return false; };
        }
    }

    FilterFunc build_doit_func(EditSelectOpType op)
    {
        switch (op)
        {
        case EditSelectOpType::Add:
            return [](u8 state) { return state | SELECT_STATE; };
        case EditSelectOpType::Remove:
            return [](u8 state) { return state & ~SELECT_STATE; };
        case EditSelectOpType::Set:
            return [](u8 state) { return state ^ SELECT_STATE; };
        default:
            return [](u8 state) { return state ^ SELECT_STATE; };
        }
    }

    FilterFunc build_undoit_func(EditSelectOpType op)
    {
        switch (op)
        {
        case EditSelectOpType::Add:
            return [](u8 state) { return state & ~SELECT_STATE; };
        case EditSelectOpType::Remove:
            return [](u8 state) { return state | SELECT_STATE; };
        case EditSelectOpType::Set:
            return [](u8 state) { return state ^ SELECT_STATE; };
        default:
            return [](u8 state) { return state ^ SELECT_STATE; };
        }
    }
    SplatSelectionOp::SplatSelectionOp(GaussianComponent* splat,EditSelectOpType op,FilterFunc pred)
        : SplateStateOp(splat,
                    build_filter_func(splat, op, pred),
                    build_doit_func(op),
                    build_undoit_func(op))
    {
    }

    SplatEntityTransformOp::SplatEntityTransformOp(GaussianComponent* splat, 
        const maths::Transform& old_trans,
        const maths::Transform& new_trans,
        maths::Transform* splat_transform)
        : SplatEditOperation(splat), 
        old_transform(old_trans), 
        new_transform(new_trans),
        transform(splat_transform)
    {
    }

    auto SplatEntityTransformOp::apply()->void
    {
        *transform = new_transform;
        splat->ModelRef->make_selection_bound_dirty();
    }

    auto SplatEntityTransformOp::undo()->void
    {
        *transform = old_transform;
        splat->ModelRef->make_selection_bound_dirty();
    }

    SplatTransformOp::SplatTransformOp(GaussianComponent* splat,
        const maths::Transform& old_trans,
        const maths::Transform& new_trans,
        const  glm::mat4& t,
        const std::unordered_map<u32, u32>& palette_map_p)
        : SplatEditOperation(splat), 
        old_transform(old_trans), 
        new_transform(new_trans),
        transform_matrix(t),
        palette_map(palette_map_p)
    {
        auto model = splat->ModelRef.get();
        indices = build_index(model, [splat](int i) { return splat->ModelRef->state()[i] == SELECT_STATE; });
    }

    auto SplatTransformOp::apply()->void
    {
        auto delta_matrix = new_transform.get_world_matrix() * glm::inverse(old_transform.get_world_matrix());
        for (auto [old_id,new_id] : palette_map)
        {
            glm::mat4 t = splat->ModelRef->splat_transforms[old_id];
            t = glm::inverse(transform_matrix) * delta_matrix * transform_matrix * glm::transpose(t);
            splat->ModelRef->splat_transforms.set_transform(new_id, t);
        }
        splat->ModelRef->make_selection_bound_dirty();
    }

    auto SplatTransformOp::undo()->void
    {
        std::unordered_map<u32,u32> inverse_map;
        for (auto [old_id, new_id] : palette_map)
            inverse_map[new_id] = old_id;
        auto& state = splat->ModelRef->state();
        auto& transform_index = splat->ModelRef->transform_index();
        for (auto i = 0; i < state.size(); i++) {
            if (state[i] == SELECT_STATE) {
                transform_index[i] = inverse_map[transform_index[i]];
            }
        }
        splat->ModelRef->update_transform_index();
        for (auto [old_id, new_id] : palette_map)
        {
            glm::mat4 t = splat->ModelRef->splat_transforms[old_id];
            splat->ModelRef->splat_transforms.set_transform(new_id, glm::transpose(t));
        }
        splat->ModelRef->make_selection_bound_dirty();
    }

    PlacePivotOp::PlacePivotOp(GaussianComponent* splat,
        const maths::Transform& old_trans,
        const maths::Transform& new_trans,
        Pivot* pivot_t)
        : SplatEditOperation(splat), 
        old_transform(old_trans), 
        new_transform(new_trans),
        pivot(pivot_t)
    {
    }

    auto PlacePivotOp::apply()->void
    {
        pivot->transform = new_transform;
    }

    auto PlacePivotOp::undo()->void
    {
        pivot->transform = old_transform;
    }

    MultiOp::MultiOp(GaussianComponent* splat,
        const std::vector<std::shared_ptr<SplatEditOperation>>& op)
        : SplatEditOperation(splat),
        ops(op)
    {
    }

    auto MultiOp::apply()->void
    {
        for(auto& op : ops)
            op->apply();
    }

    auto MultiOp::undo()->void
    {
        for (auto& op : ops)
            op->undo();
    }

    AddSplatOp::AddSplatOp(GaussianComponent* splat,Scene* sc)
        : SplatEditOperation(splat), 
        indices(build_index(splat->ModelRef.get(), [splat](int i) { 
            return splat->ModelRef->state()[i] == SELECT_STATE; })),
        scene(sc)
    {
        transform = glm::transpose(splat->ModelRef->splat_transforms.back());
    }

    auto AddSplatOp::apply()->void
    {
        new_splat_ent = scene->create_entity();
        auto& splat_com = new_splat_ent.add_component<GaussianComponent>();
        auto& modelRef = splat_com.ModelRef;
        modelRef->merge(splat->ModelRef.get(), indices);
        parallel_for<size_t>(0, indices.size(), [&](size_t i) {
			modelRef->transform_index()[i] = 0;
		});
        modelRef->update_state();
        auto& new_transform = new_splat_ent.get_or_add_component<maths::Transform>();
        new_transform = transform;
        modelRef->splat_transforms.set_transform(0, transform);
    }

    auto AddSplatOp::undo()->void
    {
        if(new_splat_ent.valid())
            new_splat_ent.destroy();
    }

    DuplicateSelectionSplatOp::DuplicateSelectionSplatOp(GaussianComponent* splat)
        : SplatEditOperation(splat),
        select_indices(build_index(splat->ModelRef.get(), [splat](int i) {
        return splat->ModelRef->state()[i] == SELECT_STATE; }))
    {
    }

    auto DuplicateSelectionSplatOp::apply()->void
    {
        auto model = splat->ModelRef.get();
        //unselect the indices splat
        duplicate_indices = model->merge(model, select_indices);
        parallel_for<size_t>(0, select_indices.size(), [&](size_t i) {
            model->state()[select_indices[i]] &= ~SELECT_STATE;
        });
        model->update_state();
    }

    auto DuplicateSelectionSplatOp::undo()->void
    {
        auto model = splat->ModelRef.get();
        parallel_for<size_t>(0, select_indices.size(), [&](size_t i) {
            model->state()[select_indices[i]] |= SELECT_STATE;
        });
        model->remove(duplicate_indices);
    }
    
    SetSplatColorAdjustmentOp::SetSplatColorAdjustmentOp(GaussianComponent* splat,const SplatColorAdjustment& old,const SplatColorAdjustment& new_state)
        : SplatEditOperation(splat), old_state(old), new_state(new_state)
    {
    }

    auto SetSplatColorAdjustmentOp::apply()->void
    {
        splat->albedo_color = new_state.albedo_color;
        splat->brightness = new_state.brightness;
        splat->transparency = new_state.transparency;
        splat->white_point = new_state.white_point;
        splat->black_point = new_state.black_point;
    }

    auto SetSplatColorAdjustmentOp::undo()->void
    {
        splat->albedo_color = old_state.albedo_color;
        splat->brightness = old_state.brightness;
        splat->transparency = old_state.transparency;
        splat->white_point = old_state.white_point;
        splat->black_point = old_state.black_point;
    }
    FilterFunc build_paint_filter_func(GaussianComponent* splat, EditSelectOpType op, FilterFunc filter)
    {
        switch (op)
        {
        case EditSelectOpType::Add:
            return [splat, filter](int i) { return (splat->ModelRef->state()[i] == 0) && filter(i); };
        case EditSelectOpType::Remove:
            return [splat, filter](int i) { return (splat->ModelRef->state()[i] == PAINT_STATE) && filter(i); };
        case EditSelectOpType::Set:
            return [splat, filter](int i) { return (splat->ModelRef->state()[i] == PAINT_STATE) != filter(i); };
        default:
            return [splat, filter](int i) { return filter(i); };
        }
    }
    SplatPaintColorAdjustmentOp::SplatPaintColorAdjustmentOp(
            GaussianComponent* splat, 
            FilterFunc pred,
            const SplatPaintColorAdjustment& newstate)
        : SplatEditOperation(splat), new_state(newstate),
        indices(build_index(splat->ModelRef.get(), build_paint_filter_func(splat, EditSelectOpType::Defalut, pred)))
    {
    }

    auto SplatPaintColorAdjustmentOp::apply()->void
    {
        auto ModelRef = splat->ModelRef.get();
        parallel_for<size_t>(0, indices.size(), [&](size_t i) {
            auto idx = indices[i];
            auto& gs_state = ModelRef->state()[idx];
            gs_state |= PAINT_STATE;
        });
        const float SH0 = 0.282094791773878f;
        const auto to = [=](f32 value) {return value * SH0 + 0.5f;};
        const auto from = [=](f32 value) {return (value - 0.5f) / SH0;};
        
        parallel_for<size_t>(0, indices.size(),[&](size_t idx){
            auto i = indices[idx];
            auto& f_dc_0 = ModelRef->sh()[i][0];
            auto& f_dc_1 = ModelRef->sh()[i][1];
            auto& f_dc_2 = ModelRef->sh()[i][2];
            f_dc_0 = from(to(f_dc_0) * (1 - new_state.mix_weight) + new_state.color.x * new_state.mix_weight);
            f_dc_1 = from(to(f_dc_1) * (1 - new_state.mix_weight) + new_state.color.y * new_state.mix_weight);
            f_dc_2 = from(to(f_dc_2) * (1 - new_state.mix_weight) + new_state.color.z * new_state.mix_weight);
        });
        ModelRef->update_state();
        ModelRef->update_feature_dc_data(indices);
    }

    auto SplatPaintColorAdjustmentOp::undo()->void
    {
        auto ModelRef = splat->ModelRef.get();
        parallel_for<size_t>(0, indices.size(), [&](size_t i) {
            auto idx = indices[i];
            auto& gs_state = ModelRef->state()[idx];
            gs_state &= ~PAINT_STATE;
        });
        const float SH0 = 0.282094791773878f;
        const auto to = [=](f32 value) {return value * SH0 + 0.5f;};
        const auto from = [=](f32 value) {return (value - 0.5f) / SH0;};
        constexpr auto eps = 1.0f / 256.0f;
        parallel_for<size_t>(0, indices.size(),[&](size_t idx){
            auto i = indices[idx];
            auto& f_dc_0 = ModelRef->sh()[i][0];
            auto& f_dc_1 = ModelRef->sh()[i][1];
            auto& f_dc_2 = ModelRef->sh()[i][2];
            f_dc_0 = from((to(f_dc_0) - new_state.color.x * new_state.mix_weight) / std::max<f32>(1 - new_state.mix_weight, eps));
            f_dc_1 = from((to(f_dc_1) - new_state.color.y * new_state.mix_weight) / std::max<f32>(1 - new_state.mix_weight, eps));
            f_dc_2 = from((to(f_dc_2) - new_state.color.z * new_state.mix_weight) / std::max<f32>(1 - new_state.mix_weight, eps));
        });
        ModelRef->update_state();
        ModelRef->update_feature_dc_data(indices);
    }
}
