#pragma once
#include <maths/transform.h>
#include <scene/component/gaussian_component.h>
#include <scene/entity.h>
#include "edit_op.h"
namespace diverse
{
    enum class EditSelectOpType
    {
        Set = 0,
        Remove = 1,
        Add = 2,
        Defalut = 3
    };

    struct SplatEditOperation : public EditOperation
    {
        SplatEditOperation(GaussianComponent* model) : splat(model) { }
        virtual void apply() {}
        virtual void undo() {}
        GaussianComponent* splat = nullptr;
    };

    using FilterFunc = std::function<bool(int)>;
    using UndoFunc = std::function<u8(u8)>;
    using DoFunc = std::function<u8(u8)>;

    struct SplateStateOp : public SplatEditOperation
    {
        SplateStateOp(GaussianComponent* splat,FilterFunc pred,DoFunc redo,UndoFunc undo);
        void apply() override;
        void undo() override;
        std::vector<u32> indices;
        FilterFunc pred;
        DoFunc doIt;
        UndoFunc undoIt;
    };
      
    struct SplatSelectAllOp : public SplateStateOp
    {
        SplatSelectAllOp(GaussianComponent* splat);
    };

    struct SplatSelectInverseOp : public SplateStateOp
    {
        SplatSelectInverseOp(GaussianComponent* splat);
    };

    struct SplatSelectNoneOp : public SplateStateOp
    {
        SplatSelectNoneOp(GaussianComponent* splat);
    };

    struct SplatSelectionOp : public SplateStateOp
    {
        SplatSelectionOp(GaussianComponent* splat,EditSelectOpType op,FilterFunc pred);
    };

    struct HidenSplatOp : public SplateStateOp
    {
        HidenSplatOp(GaussianComponent* splat);
    };
    struct UnHidenSplatOp : public SplateStateOp
    {
        UnHidenSplatOp(GaussianComponent* splat);
    };

    struct DeleteSplatEditOp : public SplateStateOp
    {
        DeleteSplatEditOp(GaussianComponent* splat);
    };

    struct ResetSplatEditOp : public SplateStateOp
    {
        ResetSplatEditOp(GaussianComponent* splat);
    };

    //splat ent transform operation
    struct SplatEntityTransformOp : public SplatEditOperation
    {
        SplatEntityTransformOp(GaussianComponent* splat, 
                            const maths::Transform& old_transform,
                            const maths::Transform& new_transform,
                            maths::Transform* splat_transform
                            );
        void apply() override;
        void undo() override;
        maths::Transform old_transform;
        maths::Transform new_transform;
        maths::Transform* transform;
    };

    //select per splat transform op
    struct SplatTransformOp : public SplatEditOperation
    {
        SplatTransformOp(GaussianComponent* splat,
                        const maths::Transform& old_transform,
                        const maths::Transform& new_transform,
                        const glm::mat4& t,
                        const std::unordered_map<u32,u32>& palette_map);
        void apply() override;
        void undo() override;
        maths::Transform old_transform;
        maths::Transform new_transform;
        glm::mat4    transform_matrix;
        std::vector<u32>    indices;
        std::unordered_map<u32, u32> palette_map;
    };
    struct PlacePivotOp : public SplatEditOperation
    {
        PlacePivotOp(GaussianComponent* splat,
                    const maths::Transform& old_transform,
                    const maths::Transform& new_transform,
                    class Pivot* pivot_t);
        void apply() override;
        void undo() override;
        maths::Transform old_transform;
        maths::Transform new_transform;
        class Pivot* pivot = nullptr;
    };

    struct MultiOp : public SplatEditOperation
    {
        MultiOp(GaussianComponent* splat,const std::vector<std::shared_ptr<SplatEditOperation>>& ops);
        void apply() override;
        void undo() override;
        std::vector<std::shared_ptr<SplatEditOperation>> ops;
    };

    struct AddSplatOp : public SplatEditOperation
    {
        AddSplatOp(GaussianComponent* splat,Scene* scene);
        void apply() override;
        void undo() override;
        std::vector<u32> indices;
        Entity new_splat_ent;
        Scene* scene;
        glm::mat4 transform;
    };
    struct DuplicateSelectionSplatOp : public SplatEditOperation
    {
        DuplicateSelectionSplatOp(GaussianComponent* splat);
        void apply() override;
        void undo() override;
        std::vector<u32> select_indices;
        std::vector<u32> duplicate_indices;
    };

    struct SplatColorAdjustment
    {
        glm::vec3 albedo_color;
        f32       brightness;
        f32       transparency;
        f32       white_point;
        f32       black_point;
    };

    struct SetSplatColorAdjustmentOp : public SplatEditOperation
    {
        SetSplatColorAdjustmentOp(GaussianComponent* splat,const SplatColorAdjustment& old,const SplatColorAdjustment& new_state);
        void apply() override;
        void undo() override;
        SplatColorAdjustment new_state;
        SplatColorAdjustment old_state;
    };

    struct SplatPaintColorAdjustment
    {
        glm::vec3 color;
        f32 mix_weight;
    };

    struct SplatPaintColorAdjustmentOp : public SplatEditOperation
    {
        SplatPaintColorAdjustmentOp(
            GaussianComponent* splat,
            FilterFunc pred,
            const SplatPaintColorAdjustment& new_state);
        void apply() override;
        void undo() override;
        std::vector<u32> indices;
        SplatPaintColorAdjustment new_state;
    };
}
