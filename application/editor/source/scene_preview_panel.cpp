#include "scene_preview_panel.h"
#include "editor.h"
#include <scene/scene.h>
#include <scene/entity_manager.h>
#include <imgui.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <imgui/imgui_helper.h>
#include <imgui/IconsMaterialDesignIcons.h>
#include <stb/image_utils.h>
#include <format>
#include <json/json.hpp>

#define COL_SLOT_ODD        IM_COL32( 58,  58,  58, 255)
#define COL_SLOT_EVEN       IM_COL32( 64,  64,  64, 255)

using json = nlohmann::json;

namespace diverse
{
	ScenePreviewPanel::ScenePreviewPanel(bool active)
		:EditorPanel(active)
	{
		m_Name = U8CStr2CStr(ICON_MDI_VIEW_LIST " ScenePreview###ScenePreview");
		m_SimpleName = "ScenePreview";
	}

	void ScenePreviewPanel::on_imgui_render()
	{
		//DS_PROFILE_FUNCTION();
		Application& app = Application::get();

		ImGuiHelper::ScopedStyle windowPadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		auto flags = ImGuiWindowFlags_NoCollapse;

		if (!ImGui::Begin(m_Name.c_str(), &m_Active, flags) )
		{
			ImGui::End();
			return;
		}
		ImGuiIO& io = ImGui::GetIO();
		int cx = (int)(io.MousePos.x);
		int cy = (int)(io.MousePos.y);
        const int toolbar_height = 24;
		constexpr float DummySpace = 3.0f;
		bool overTopBar = false;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 window_pos = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(window_pos + ImVec2(28, 0));
		ImVec2 window_size = ImGui::GetWindowSize();
		ImVec2 canvas_pos = ImGui::GetCursorScreenPos();                    // ImDrawList API uses screen coordinates!
		ImVec2 canvas_size = ImGui::GetContentRegionAvail() - ImVec2(8, 0); // resize canvas to what's available

		ImGui::BeginGroup();
		bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

		ImGui::SetCursorScreenPos(canvas_pos);

		ImVec2 toolBarSize(canvas_size.x, (float)toolbar_height);
		ImRect ToolBarAreaRect(canvas_pos, canvas_pos + toolBarSize);
		ImVec2 HeaderPos = ImGui::GetCursorScreenPos() + ImVec2(0, toolbar_height);

		ImGui::TextUnformatted("CameraLens List");
		ImGui::SameLine();

		//draw_list->AddRectFilled(ToolBarAreaRect.Min, ToolBarAreaRect.Max, , 0);
		ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.8f) - (1.5f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));
		{
			if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLUS)))
			{
				auto renderTarget = m_Editor->get_main_render_texture();
				CamLenPreviewImage node;
				node.tex = m_Editor->get_imgui_manager()->get_imgui_renderer()->create_texture(256,256);
				node.name = "view" + std::to_string(m_PreviewSets->previewImages.size());
				m_Editor->get_imgui_manager()->get_imgui_renderer()->blit_texture(renderTarget.get(), node.tex.get());

				auto viewMat = m_Editor->get_editor_camera_transform().get_local_matrix();
				node.cameraVar = CameraKeyFrameVar(viewMat, 1, 1, 1, 1);
				m_PreviewSets->previewImages.push_back(node);
			}
			if (ImGui::IsItemHovered())
				ImGuiHelper::Tooltip("add a preview");
			ImGui::SameLine();
		}
		draw_list->AddLine(ImVec2(0, ToolBarAreaRect.Min.y + toolbar_height - 1), ToolBarAreaRect.Max + ImVec2(4, -1), IM_COL32(128, 128, 128, 224));
		ImGui::PopStyleColor();

		ImGui::SetCursorScreenPos(HeaderPos);
		auto nodePos = HeaderPos + ImVec2(0, window_size.y * 0.02);
		ImVec2 imgSize = ImVec2(window_size.x * 0.5, window_size.y * 0.08);
		ImVec2 imgRowSize = ImVec2(window_size.x, window_size.y * 0.12);
		ImVec2 imgRowPos = ImVec2(0, HeaderPos.y);
		CamLenPreviewImage*	deleteItem = nullptr;
		for (auto node_id = 0;node_id < m_PreviewSets->previewImages.size(); node_id++)
		{
			auto& node = m_PreviewSets->previewImages[node_id];
			unsigned int col = ImColor(4,4,4,255);
			ImRect itemRect = ImRect(imgRowPos, imgRowPos + imgRowSize);
			bool overItem = false;
			if (itemRect.Contains(io.MousePos) )
			{
				col += IM_COL32(8, 16, 32, 128);
				draw_list->AddRectFilled(imgRowPos, imgRowPos + imgRowSize, col, 0);
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					set_selected(&node);
				}
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					m_state = CamLenPreviewState::Switching;
				}

				overItem = true;
			}
			draw_list->AddImage(reinterpret_cast<ImTextureID>(add_imgui_texture(node.tex)), nodePos, nodePos + imgSize);

			ImGui::SetCursorScreenPos(imgRowPos + ImVec2(window_size.x * 0.8, imgRowSize.y / 2 - ImGui::GetFontSize() * 0.5));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));
			ImGui::PushID(node_id);
			if(ImGui::Button(U8CStr2CStr((ICON_MDI_REFRESH))) )
			{
				auto renderTarget = m_Editor->get_main_render_texture();
				m_Editor->get_imgui_manager()->get_imgui_renderer()->blit_texture(renderTarget.get(), node.tex.get());
			}
			ImGui::PopID();

			ImGuiHelper::Tooltip("Refresh");
			ImGui::PopStyleColor();

			if (overItem && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
			{
				ImGui::OpenPopup("##previewnode-context-menu");
			}
			if (overItem && ImGui::BeginPopup("##previewnode-context-menu"))
			{
				ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0, 1.0, 1.0, 1.0));
				ImGui::Dummy(ImVec2(0.0f, DummySpace));
				if (ImGui::MenuItem(" switch "))
				{
					set_selected(&node);
				}
				ImGui::Dummy(ImVec2(0.0f, DummySpace));
				if (ImGui::MenuItem(" delete "))
				{
					deleteItem = &node;
				}
				ImGui::Dummy(ImVec2(0.0f, DummySpace));
				ImGui::PopStyleColor();

				ImGui::EndPopup();
			}
			imgRowPos.y += imgRowSize.y;
			nodePos.y += imgSize.y + window_size.y * 0.04;
		}
		if (deleteItem)
			delete_preview(deleteItem);

		ImGui::EndGroup();
		ImGui::End();
	}

	void ScenePreviewPanel::on_update(float dt)
	{
		static float curPlayTime = 0.0;
		if (curPlayTime > 1.0f)
		{
			m_state = CamLenPreviewState::Normal;
			curPlayTime = 0.0;
		}
		if (m_state == CamLenPreviewState::Switching)
		{
			curPlayTime += 0.02;
			auto viewMat = m_Editor->get_editor_camera_transform().get_local_matrix();
			auto curVar = CameraKeyFrameVar((viewMat), 1, 1, 1, 1);

			auto k = lerp(curVar, m_SelectNode->cameraVar, curPlayTime, 0.0f, 1.0f);
			auto& camera_transform = m_Editor->get_editor_camera_transform();
			m_Editor->get_editor_camera_controller().update_focal_point(camera_transform, k.T);
			camera_transform.set_local_orientation(k.R);
			camera_transform.set_world_matrix(glm::mat4(1.0f));
		}
	}


	void ScenePreviewPanel::draw_preview_node(CamLenPreviewImage*  node)
	{
		bool show = true;
		const char* name = node->name.c_str();
  		if(show)
        {
		}
	}

	bool ScenePreviewPanel::is_selected(CamLenPreviewImage* node)
	{
		if (node == m_SelectNode)
			return true;

		return false;
	}

	void ScenePreviewPanel::set_selected(CamLenPreviewImage* node)
	{
		m_SelectNode = node;
	}

	void ScenePreviewPanel::delete_preview(CamLenPreviewImage* node)
	{
		auto it = std::find_if(m_PreviewSets->previewImages.begin(), m_PreviewSets->previewImages.end(), [&](const CamLenPreviewImage& other)->bool {return *node == other; });
		if (it != m_PreviewSets->previewImages.end())
		{
			m_PreviewSets->previewImages.erase(it);
		}
	}

	void ScenePreviewPanel::add_camera_view(const glm::mat4& viewMat)
	{
		if(m_PreviewSets)
		{
			auto renderTarget = m_Editor->get_main_render_texture();
			CamLenPreviewImage node;
			node.tex = m_Editor->get_imgui_manager()->get_imgui_renderer()->create_texture(256,256);
			node.name = "view" + std::to_string(m_PreviewSets->previewImages.size());
			m_Editor->get_imgui_manager()->get_imgui_renderer()->blit_texture(renderTarget.get(), node.tex.get());

			node.cameraVar = CameraKeyFrameVar(viewMat, 1, 1, 1, 1);
			m_PreviewSets->previewImages.push_back(node);
		}
	}

	void ScenePreviewPanel::on_new_scene(Scene* scene)
	{
		m_SelectNode = nullptr;
		m_state = CamLenPreviewState::Normal;

		auto ents = m_Editor->get_current_scene()->get_entity_manager()->get_entities_with_type<KeyFrameTimeLine>();
		if (ents.empty() )
		{
			auto KeyFrameEnt = m_Editor->get_current_scene()->create_entity("KeyFrameEnt");
			KeyFrameEnt.add_component<KeyFrameTimeLine>();
			m_PreviewSets = &KeyFrameEnt.get_or_add_component<CamLenPreviewImageSet>();
		}
		else
		{
			auto ent = ents.front();
			m_PreviewSets = &ent.get_or_add_component< CamLenPreviewImageSet>();
		}
	}
	struct JsonCameraFrame
	{
		std::vector<std::vector<float>> rotation;
		std::vector<float>  position;
		std::string name;
	};
	struct JsonCmeraTransforms
	{
		std::vector<JsonCameraFrame> transforms;
	};
	void from_json(const json& j, JsonCameraFrame& t) {
		if(j.contains("rotation")){
			j.at("rotation").get_to(t.rotation);
			j.at("position").get_to(t.position);
		}
		if (j.contains("img_name")) j.at("img_name").get_to(t.name);
	}

	void from_json(const json& j, JsonCmeraTransforms& t) {
		j.get_to(t.transforms);
	}
	JsonCmeraTransforms readCameraTransforms(const std::string& filename) {
		std::ifstream f(filename);
		json data = json::parse(f);
		f.close();
		return data.template get<JsonCmeraTransforms>();
	}

	bool ScenePreviewPanel::handle_file_drop(WindowFileEvent& e)
	{
		//const auto file_paths = e.GetFilePaths();
  //      if (file_paths.size() == 1)
  //      {
		//	auto path = utf8_to_multibyte(file_paths[0]);
  //      	path = stringutility::back_slashes_2_slashes(path);
		//	if(m_Editor->is_camera_json_file(path))
		//	{
		//		JsonCmeraTransforms ts = readCameraTransforms(path);
		//		glm::vec3 ave(0.0f);
		//		for (auto& t : ts.transforms)
		//		{
		//			glm::vec3 position;
		//			for (int i = 0; i < 3; i++) {
		//				position[i] = t.position[i];
		//			}
		//			ave += position;
		//		}
		//		ave /= ts.transforms.size();

		//		for (auto& t : ts.transforms)
		//		{
		//			glm::vec3 position;
		//			glm::mat3 rotation;
		//			for (int i = 0; i < 3; i++) {
		//				position[i] = t.position[i];
		//				for (int j = 0; j < 3; j++) {
		//					rotation[i][j] = t.rotation[j][i];
		//				}
		//			}
		//			// auto z = glm::vec3(rotation[2][0], rotation[2][1], rotation[2][2]);
		//			// auto p = position;
		//			// auto dot = glm::dot(ave -p,z);
		//			// auto vec = z * dot + p;
		//			// auto pos = glm::vec3(-p.x,-p.y,p.z);
		//			// auto target = glm::vec3(-vec.x,-vec.y,vec.z);
		//			// vec = target - pos;
		//			// auto l = glm::length(vec);
		//			// auto azim = atan2(-vec.x/l, -vec.z / l);
		//			// auto elev = asin(vec.y/l);
		//			maths::Transform cameraTrans;
		//			//cameraTrans.set_local_orientation(glm::vec3(elev,azim,0));
		//			cameraTrans.set_local_orientation(rotation);
		//			// glm::vec3 forward = cameraTrans.get_local_orientation() * glm::vec3(0.0f, 0.0f, 1.0f);
		//			// auto camPos = target + forward * 1.0f;
		//			cameraTrans.set_local_position(position);
		//			cameraTrans.set_world_matrix(glm::mat4(1.0f));
		//			add_camera_view(cameraTrans.get_world_matrix());
		//		}
		//	}
  //      }
		return true;
	}

	auto CamLenPreviewImage::load_image() -> void
	{
		int width, height, comp;
		auto img_data = load_stbi(Application::get().get_project_settings().m_ProjectRoot + "assets/previews/" + name + ".png", &width, &height, &comp, 4);
		tex = Application::get().get_imgui_manager()->get_imgui_renderer()->create_texture(width, height,img_data);
		free(img_data);
	}

	auto CamLenPreviewImage::save_image() const-> void
	{
		auto img_data = Application::get().get_imgui_manager()->get_imgui_renderer()->export_texture(tex.get());
		auto path = Application::get().get_project_settings().m_ProjectRoot + "assets/previews/";
		write_stbi(path + name + ".png", 256, 256, 4, (uint8_t*)img_data.data(), 100);
	}
}