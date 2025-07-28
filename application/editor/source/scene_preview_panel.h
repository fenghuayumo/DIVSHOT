#pragma once
#include "editor_panel.h"
#include "scene/component/time_line.h"
namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }
    struct CamLenPreviewImage
    {
        //uint64 texID;
        std::shared_ptr<rhi::GpuTexture>  tex = nullptr;
        //int64  nodeID;
        std::string name;

        CameraKeyFrameVar   cameraVar;
        bool operator==(const CamLenPreviewImage& other) const
        {
            return tex.get() == other.tex.get();
        }

        auto load_image()->void;
        auto save_image() const ->void;
        template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("previewname", name), cereal::make_nvp("previewCamera", cameraVar));
            save_image();
        }
        template <typename Archive>
        void load(Archive& archive)
        {
            archive(cereal::make_nvp("previewname", name),cereal::make_nvp("previewCamera", cameraVar));
            load_image();
        }
    };

    struct CamLenPreviewImageSet
    {
		std::vector<CamLenPreviewImage>  previewImages;
		template <typename Archive>
        void save(Archive& archive) const
        {
            archive(cereal::make_nvp("previewImagesize", previewImages.size()));
            for (auto i = 0; i < previewImages.size(); i++)
				archive(cereal::make_nvp("previewImage"+std::to_string(i), previewImages[i]));
		}
		template <typename Archive>
        void load(Archive& archive)
        {
            int img_size = 0;
            archive(cereal::make_nvp("previewImagesize", img_size));
            previewImages.resize(img_size);
            for (auto i = 0; i < previewImages.size(); i++)
                archive(cereal::make_nvp("previewImage" + std::to_string(i), previewImages[i]));
		}
	};
    enum  CamLenPreviewState
    {
        Normal,
        Switching
    };

    class ScenePreviewPanel : public EditorPanel
    {
	public:
        ScenePreviewPanel(bool active = false);
		void	on_imgui_render() override;
        void    on_update(float dt) override;
        void    draw_preview_node( CamLenPreviewImage* node);
        bool    is_selected(CamLenPreviewImage* node);
        void    set_selected(CamLenPreviewImage* node);
        void    delete_preview(CamLenPreviewImage* node);
        void    add_camera_view(const glm::mat4& view);
        void    on_new_scene(Scene* scene) override;
        bool    handle_file_drop(WindowFileEvent& e) override;
    protected:
        CamLenPreviewImageSet*  m_PreviewSets;
        CamLenPreviewImage* m_SelectNode;

        CamLenPreviewImage* m_DoubleClicked;
        //CamLenPreviewImage m_HadRecentDropped;
        CamLenPreviewImage* m_CurrentPrevious;
        bool m_SelectUp;
        bool m_SelectDown;

        CamLenPreviewState  m_state = CamLenPreviewState::Normal;
    };
}