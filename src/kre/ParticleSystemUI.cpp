/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <memory>

#include "CameraObject.hpp"
#include "ParticleSystemUI.hpp"
#include "ParticleSystem.hpp"
#include "ParticleSystemEmitters.hpp"
#include "ParticleSystemAffectors.hpp"
#include "ParticleSystemParameters.hpp"
#include "WindowManager.hpp"

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"
#endif

namespace ImGui
{
	int Curve(const char *label, const ImVec2& size, int maxpoints, ImVec2 *points);
	float CurveValue(float p, int maxpoints, const ImVec2 *points);

	bool Spline(const char *label, const ImVec2& size, int maxpoints, ImVec2 *points);
}

namespace KRE
{
	namespace Particles
	{
		void ParameterGui(const char* label, const KRE::Particles::ParameterPtr& param, float fmin = 0.0f, float fmax = 0.0f)
		{
			// XXX we need to deal with this condition. Probably add another option to type.
			if(param == nullptr) {
				return;
			}

			using namespace KRE::Particles;
			ImGui::Text("%s", label);
			const char* const ptype[] = { "Fixed", "Random", "Linear", "Spline", "Oscillate" };
			int current_type = static_cast<int>(param->getType());
			std::string combo_label = "Type##";
			combo_label += label;
			if(ImGui::Combo(combo_label.c_str(), &current_type, ptype, 5)) {
				param->setType(static_cast<ParameterType>(current_type));
			}

			float v_speed = 1.0f;
			if(fmax != 0.0f) {
				v_speed = fmax / 100.0f;
			}

			switch(param->getType()){
			case ParameterType::FIXED: {
				FixedParams fp;
				param->getFixedValue(&fp);
				std::string fixed_label = "Value##";
				fixed_label += label;
				if(ImGui::DragFloat(fixed_label.c_str(), &fp.value, v_speed, fmin, fmax)) {
					param->setFixedValue(fp);
				}
				break;
			}
			case ParameterType::RANDOM: {
				RandomParams rp;
				param->getRandomRange(&rp);
				std::string min_label = "Min Value##";
				min_label += label;
				if(ImGui::DragFloat(min_label.c_str(), &rp.min_value, v_speed, fmin, fmax)) {
					param->setRandomRange(rp);
				}
				std::string max_label = "Max Value##";
				max_label += label;
				if(ImGui::DragFloat(max_label.c_str(), &rp.max_value, v_speed, fmin, fmax)) {
					param->setRandomRange(rp);
				}
				break;
			}
			case ParameterType::CURVED_LINEAR: {
				CurvedParams cp;
				param->getCurvedParams(&cp);
				ImVec2 points[10];
				std::fill(points, points+10, ImVec2(-1.0f, 0.0f));
				if(cp.control_points.size() < 2) {
					points[0].x = -1.0f;
				} else {
					int n = 0;
					for(auto& p : cp.control_points) {
						points[n].x = static_cast<float>(p.first);
						points[n].y = static_cast<float>(p.second);
						if(++n >= 10) {
							break;
						}
					}
				}
				std::string linear_label = "Linear##";
				linear_label += label;
				if(ImGui::Curve(linear_label.c_str(), ImVec2(300, 200), 10, points)) {
					cp.control_points.clear();
					for(int n = 0; n != 10 && points[n].x >= 0; ++n) {
						cp.control_points.emplace_back(points[n].x, points[n].y);
					}
					param->setControlPoints(InterpolationType::LINEAR,  cp);
				}
				break;
			}
			case ParameterType::CURVED_SPLINE: {
				CurvedParams cp;
				param->getCurvedParams(&cp);
				ImVec2 points[10];
				std::fill(points, points+10, ImVec2(-1.0f, 0.0f));
				if(cp.control_points.size() < 2) {
					points[0].x = -1.0f;
				} else {
					int n = 0;
					for(auto& p : cp.control_points) {
						points[n].x = static_cast<float>(p.first);
						points[n].y = static_cast<float>(p.second);
						if(++n >= 10) {
							break;
						}
					}
				}
				std::string spline_label = "Spline##";
				spline_label += label;
				if(ImGui::Spline(spline_label.c_str(), ImVec2(300.0f, 200.0f), 10, points)) {
					cp.control_points.clear();
					for(int n = 0; n != 10 && points[n].x >= 0; ++n) {
						cp.control_points.emplace_back(points[n].x, points[n].y);
					}
					param->setControlPoints(InterpolationType::SPLINE,  cp);
				}
				break;
			}

			case ParameterType::OSCILLATE: {
				OscillationParams op;
				param->getOscillation(&op);
				const char* const osc_items[] = { "Sine", "Square" };
				int otype = static_cast<int>(op.osc_type);
				std::string wtype_label = "Wave Type##";
				wtype_label += label;
				if(ImGui::Combo(wtype_label.c_str(), &otype, osc_items, 2)) {
					op.osc_type = static_cast<WaveType>(otype);
					param->setOscillation(op);
				}
				std::string freq_label = "Frequency##";
				freq_label += label;
				if(ImGui::DragFloat(freq_label.c_str(), &op.frequency, 1.0f, 1.0f, 10000.0f)) {
					param->setOscillation(op);
				}
				std::string phase_label = "Phase##";
				phase_label += label;
				if(ImGui::DragFloat(phase_label.c_str(), &op.phase, 1.0f, 0.0f, 360.0f)) {
					param->setOscillation(op);
				}
				std::string base_label = "Base##";
				base_label += label;
				if(ImGui::DragFloat(base_label.c_str(), &op.base, 1.0f, 0.0f, 1000.0f)) {
					param->setOscillation(op);
				}
				std::string amplitude_label = "Amplitude##";
				amplitude_label += label;
				if(ImGui::DragFloat(amplitude_label.c_str(), &op.amplitude, v_speed, fmin, fmax)) {
					param->setOscillation(op);
				}
				break;
			}
			default: break;
			}
		}

		void theme_imgui()
		{
			// Setup style
			ImGuiStyle& style = ImGui::GetStyle();
			style.Colors[ImGuiCol_Text] = ImVec4(0.31f, 0.25f, 0.24f, 1.00f);
			style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
			style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.74f, 0.94f, 1.00f);
			style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.68f, 0.68f, 0.68f, 0.00f);
			style.Colors[ImGuiCol_Border] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
			style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			style.Colors[ImGuiCol_FrameBg] = ImVec4(0.62f, 0.70f, 0.72f, 0.56f);
			style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.33f, 0.14f, 0.47f);
			style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.97f, 0.31f, 0.13f, 0.81f);
			style.Colors[ImGuiCol_TitleBg] = ImVec4(0.42f, 0.75f, 1.00f, 0.53f);
			style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.65f, 0.80f, 0.20f);
			style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.40f, 0.62f, 0.80f, 0.15f);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.39f, 0.64f, 0.80f, 0.30f);
			style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.67f, 0.80f, 0.59f);
			style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 0.48f, 0.53f, 0.67f);
			style.Colors[ImGuiCol_ComboBg] = ImVec4(0.89f, 0.98f, 1.00f, 0.99f);
			style.Colors[ImGuiCol_CheckMark] = ImVec4(0.48f, 0.47f, 0.47f, 0.71f);
			style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.31f, 0.47f, 0.99f, 1.00f);
			style.Colors[ImGuiCol_Button] = ImVec4(1.00f, 0.79f, 0.18f, 0.78f);
			style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.82f, 1.00f, 0.81f);
			style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.72f, 1.00f, 1.00f, 0.86f);
			style.Colors[ImGuiCol_Header] = ImVec4(0.65f, 0.78f, 0.84f, 0.80f);
			style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.88f, 0.94f, 0.80f);
			style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.68f, 0.74f, 0.80f);
			style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.60f, 0.60f, 0.80f, 0.30f);
			style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
			style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
			style.Colors[ImGuiCol_CloseButton] = ImVec4(0.41f, 0.75f, 0.98f, 0.50f);
			style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(1.00f, 0.47f, 0.41f, 0.60f);
			style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(1.00f, 0.16f, 0.00f, 1.00f);
			style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.99f, 0.54f, 0.43f);
			style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

			style.Alpha = 1.0f;
			style.FrameRounding = 4;
			style.IndentSpacing = 12.0f;
		}

		bool vector_string_getter(void* data, int n, const char** out_text)
		{
			std::vector<std::string>* v = static_cast<std::vector<std::string>*>(data);
			*out_text = (*v)[n].c_str();
			return true;
		}

		void EmitObjectUI(const KRE::Particles::EmitObjectPtr& eo)
		{
			using namespace KRE::Particles;

			std::array<char, 64> text;
			std::fill(text.begin(), text.end(), 0);
			//std::copy(eo->getName().begin(), eo->getName().end(), std::inserter(text.begin()));
			int n = 0;
			for(auto c : eo->getName()) {
				text[n] = c;
				++n;
			}
			if(ImGui::InputText("Name", text.data(), text.size())) {
				eo->setName(text.data());
			}

			bool enabled = eo->isEnabled();
			if(ImGui::Checkbox("Enabled", &enabled)) {
				eo->setEnable(enabled);
			}

			bool debug_draw = eo->doDebugDraw();
			if(ImGui::Checkbox("Debug Draw", &debug_draw)) {
				eo->setDebugDraw(debug_draw);
			}
		}

		bool QuaternionGui(const std::string& s, glm::quat* q)
		{
			ImGui::PushID(q);
			float angle;
			glm::vec3 axis;
			ImGui::BeginGroup();
			ImGui::Text("%s", s.c_str());
			KRE::Particles::convert_quat_to_axis_angle(*q, &angle, &axis);
			angle *= static_cast<float>(180.0f / M_PI);		// radians to degrees for display
			bool changed = false;
			if(ImGui::DragFloat("Angle", &angle, 1.0f, 0.0f, 360.0f)) {
				changed |= true;
			}
			float vaxis[3]{ axis.x, axis.y, axis.z };
			if(ImGui::DragFloat3("Axis", vaxis, 0.05f, -1.0f, 1.0f)) {
				changed |= true;
			}
			if(ImGui::Button(" +X ")) {
				vaxis[0] = 1.0f;
				vaxis[1] = 0.0f;
				vaxis[2] = 0.0f;
				changed |= true;
			}
			ImGui::SameLine();
			if(ImGui::Button(" +Y ")) {
				vaxis[0] = 0.0f;
				vaxis[1] = 1.0f;
				vaxis[2] = 0.0f;
				changed |= true;
			}
			ImGui::SameLine();
			if(ImGui::Button(" +Z ")) {
				vaxis[0] = 0.0f;
				vaxis[1] = 0.0f;
				vaxis[2] = 1.0f;
				changed |= true;
			}
			if(ImGui::Button(" -X ")) {
				vaxis[0] = -1.0f;
				vaxis[1] = 0.0f;
				vaxis[2] = 0.0f;
				changed |= true;
			}
			ImGui::SameLine();
			if(ImGui::Button(" -Y ")) {
				vaxis[0] = 0.0f;
				vaxis[1] = -1.0f;
				vaxis[2] = 0.0f;
				changed |= true;
			}
			ImGui::SameLine();
			if(ImGui::Button(" -Z ")) {
				vaxis[0] = 0.0f;
				vaxis[1] = 0.0f;
				vaxis[2] = -1.0f;
				changed |= true;
			}
			if(changed) {
				*q = glm::angleAxis(angle / 180.0f * static_cast<float>(M_PI), glm::vec3(vaxis[0], vaxis[1], vaxis[2]));
			}
			ImGui::EndGroup();
			ImGui::PopID();
			return changed;
		}

		void ParticleUI(ParticleSystemContainerPtr pscontainer, bool* enable_mouselook, bool* invert_mouselook, const std::vector<std::string>& image_files)
		{
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiSetCond_Always);
			auto ps_camera = pscontainer->getCamera();
			auto& psystem = pscontainer->getParticleSystem();

			auto wnd = KRE::WindowManager::getMainWindow();
			const int neww = wnd->width();
			const int newh = wnd->height();
			const float aspect_ratio = static_cast<float>(neww) / static_cast<float>(newh);

			if(ps_camera == nullptr) {
				ps_camera = std::make_shared<Camera>("ps_camera", 0, neww, 0, newh);
				pscontainer->attachCamera(ps_camera);
			}

			ImGui::Begin("Particle System Editor");
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Text("Particle Count: %d", psystem->getParticleCount());

			if(ImGui::CollapsingHeader("Camera")) {
				std::vector<std::string> camera_types{ "Perspective", "Orthogonal" };
				int current_item = ps_camera->getType();

				if(ImGui::Combo("Type##Camera", &current_item, vector_string_getter, static_cast<void*>(&camera_types), camera_types.size())) {
					if(current_item == Camera::CAMERA_PERSPECTIVE) {
						ps_camera.reset(new Camera("ps_camera", 45.0, aspect_ratio, 0.01f, 100.0f));
						pscontainer->attachCamera(ps_camera);
					} else if(current_item == Camera::CAMERA_ORTHOGONAL) {
						ps_camera.reset(new Camera("ps_camera", 0, neww, 0, newh));
						pscontainer->attachCamera(ps_camera);
					} else {
						ASSERT_LOG(false, "Bad camera type: " << current_item);
					}
				}

				if(current_item == Camera::CAMERA_PERSPECTIVE) {
					float fov = ps_camera->getFov();
					if(ImGui::SliderFloat("Field of View", &fov, 15.0f, 115.0f)) {
						ps_camera->setFov(fov);
					}
					const glm::vec3& pos = ps_camera->getPosition();
					const glm::vec3& target = ps_camera->getTarget();
					const glm::vec3& up = ps_camera->getUp();

					bool changed = false;
					float vpos[3]{ pos.x, pos.y, pos.z };
					ImGui::PushID(ps_camera.get());
					if(ImGui::DragFloat3("Position", vpos, 1.0f, -1000.0f, 1000.0f)) {
						changed |= true;
					}
					float tpos[3]{ target.x, target.y, target.z };
					if(ImGui::DragFloat3("Target", tpos, 1.0f, -1000.0f, 1000.0f)) {
						changed |= true;
					}
					float upos[3]{ up.x, up.y, up.z };
					if(ImGui::DragFloat3("Up", upos, 0.01f, -1.0f, 1.0f)) {
						changed |= true;
					}
					ImGui::PopID();
					if(changed) {
						ps_camera->lookAt(glm::vec3(vpos[0], vpos[1], vpos[2]), glm::vec3(tpos[0], tpos[1], tpos[2]), glm::vec3(upos[0], upos[1], upos[2]));
					}

					changed = false;
					float near_clip = ps_camera->getNearClip();
					float far_clip = ps_camera->getFarClip();

					if(ImGui::DragFloat("Near Clip", &near_clip, 0.01f, 0.01f, 1.0f)) {
						changed |= true;
					}
					if(ImGui::DragFloat("Far Clip", &far_clip, 1.0f, 1.0f, 1000.0f, "%.3f", 1.2f)) {
						changed |= true;
					}
					if(changed) {
						ps_camera->setClipPlanes(near_clip, far_clip);
					}

					ImGui::Checkbox("Invert mouselook", invert_mouselook);
					if(ImGui::Checkbox("Enable mouselook", enable_mouselook)) {
						SDL_SetRelativeMouseMode(enable_mouselook ? SDL_TRUE : SDL_FALSE);
						SDL_GetRelativeMouseState(nullptr, nullptr);
					}
					ImGui::TextColored(glm::vec4(1.0f, 0.1f, 0.2f, 1.0f), "Press ESC to exit mouselook mode");

				} else if(current_item == Camera::CAMERA_ORTHOGONAL) {				
					int tbv[2]{ ps_camera->getOrthoTop(), ps_camera->getOrthoBottom() };
					if(ImGui::DragInt2("Top/Bottom", tbv, 1, 0, 4000)) {
						ps_camera->setOrthoWindow(ps_camera->getOrthoLeft(),ps_camera->getOrthoRight(), tbv[0], tbv[1]);
					}
					int lrv[2]{ ps_camera->getOrthoLeft(), ps_camera->getOrthoRight() };
					if(ImGui::DragInt2("Left/Right", lrv, 1, 0, 4000)) {
						ps_camera->setOrthoWindow(lrv[0], lrv[1], ps_camera->getOrthoTop(), ps_camera->getOrthoBottom());
					}
					if(ImGui::Button("Set to screen dimensions")) {
						ps_camera->setOrthoWindow(0, neww, 0, newh);
					}
				}
			}

			{
				if(ImGui::CollapsingHeader("Particle System")) {
					float sv = psystem->getScaleVelocity();
					if(ImGui::DragFloat("Scale Velocity", &sv, 0.5f, -100.0f, 100.0f)) {
						psystem->setScaleVelocity(sv);
					}
					float st = psystem->getScaleTime();
					if(ImGui::DragFloat("Scale Time", &st, 0.5f, 0.1f, 100.0f)) {
						psystem->setScaleTime(st);
					}
					glm::vec3 scale_dims = psystem->getScaleDimensions();
					float sd[3]{ scale_dims[0], scale_dims[1], scale_dims[2] };
					if(ImGui::DragFloat3("Scale Dimensions", sd, 0.1f, 0.1f, 100.0f)) {
						psystem->setScaleDimensions(sd);
					}

					// technique stuff
					auto dim = psystem->getDefaultDimensions();

					if(ImGui::SliderFloat("Default Width", &dim.x, 0.0f, 100.0f)) {
						psystem->setDefaultWidth(dim.x);
					}
					if(ImGui::SliderFloat("Default Height", &dim.y, 0.0f, 100.0f)) {
						psystem->setDefaultHeight(dim.y);
					}
					if(ImGui::SliderFloat("Default Depth", &dim.z, 0.0f, 100.0f)) {
						psystem->setDefaultDepth(dim.z);
					}

					int quota = psystem->getParticleQuota();
					if(ImGui::DragInt("Particle Quota", &quota, 1, 1, 100000)) {
						psystem->setParticleQuota(quota);
					}

					int current_tex = 0;
					std::string selected_texture;
					if(psystem->getTexture() != nullptr) {
						selected_texture = psystem->getTexture()->getSurface(0)->getName();
					}
					if(!image_files.empty()) {
						int current_file = 0;
						if(!selected_texture.empty()) {
							current_file = std::distance(image_files.begin(), std::find(image_files.begin(), image_files.end(), selected_texture));
						}
						ImGui::Text("%s", selected_texture.c_str());
						if(ImGui::ListBox("Textures", &current_file, vector_string_getter, const_cast<void*>(static_cast<const void*>(&image_files)), image_files.size())) {
							psystem->setTexture(Texture::createTexture(image_files[current_file]));
						}
					}

					bool has_max_velocity = psystem->hasMaxVelocity();
					if(ImGui::Checkbox("Has Max Velocity", &has_max_velocity)) {
						if(has_max_velocity) {
							psystem->setMaxVelocity(0);
						} else {
							psystem->clearMaxVelocity();
						}
					}

					if(psystem->hasMaxVelocity()) {
						float maxv = psystem->getMaxVelocity();
						if(ImGui::DragFloat("Max Velocity", &maxv, 1.0f, 0.0f, 1000.0f)) {
							psystem->setMaxVelocity(maxv);
						}
					}

					// stuff from renderable
					bool ignore_global_mm = psystem->ignoreGlobalModelMatrix();
					if(ImGui::Checkbox("Ignore Global Transform", &ignore_global_mm)) {
						psystem->useGlobalModelMatrix(ignore_global_mm);
					}
					const auto& pos = psystem->getPosition();
					float v[3]{ pos.x, pos.y, pos.z };
					if(ImGui::DragFloat3("Position", v)) {
						psystem->setPosition(v[0], v[1], v[2]);
					}
					// blend mode
					const auto& bm = psystem->getBlendMode();
					auto blend_modes = BlendMode::getBlendModeStrings();

					std::string bm_string = bm.to_string();
					auto it = std::find(blend_modes.begin(), blend_modes.end(), bm_string);
					int current_item = 0;
					if(it != blend_modes.end()) {
						current_item = std::distance(blend_modes.begin(), it);
					}

					if(ImGui::Combo("Blend Mode", &current_item, vector_string_getter, static_cast<void*>(&blend_modes), blend_modes.size())) {
						psystem->setBlendMode(BlendMode(blend_modes[current_item]));
					}

					bool depth_write = psystem->isDepthWriteEnable();
					if(ImGui::Checkbox("Depth Write", &depth_write)) {
						psystem->setDepthWrite(depth_write);
					}
					bool depth_check = psystem->isDepthEnabled();
					if(ImGui::Checkbox("Depth Check", &depth_check)) {
						psystem->setDepthEnable(depth_check);
					}
				}
			}

			Particles::EmitterPtr emitter_replace = nullptr;
			auto& e = psystem->getActiveEmitter();
			{
				if(ImGui::CollapsingHeader("Emitter")) {
					Particles::EmitterType type = e->getType();

					EmitObjectUI(e);

					static std::vector<std::string> ptype{ "Point", "Line", "Box", "Circle", "Sphere Surface" };
					int current_type = static_cast<int>(type);
					ImGui::PushID(e.get());
					if(ImGui::Combo("Type", &current_type, vector_string_getter, &ptype, ptype.size())) {
						emitter_replace = Particles::Emitter::factory(pscontainer, static_cast<Particles::EmitterType>(current_type));						
					}
					ImGui::PopID();

					ParameterGui("Emission Rate", e->getEmissionRate());
					ParameterGui("Time to live", e->getTimeToLive());
					ParameterGui("Velocity", e->getVelocity());
					ParameterGui("Angle", e->getAngle(), 0.0f, 360.0f);
					ParameterGui("Mass", e->getMass());
					ParameterGui("Duration", e->getDuration(), 0.0f, 100.0f);
					ParameterGui("Repeat Delay", e->getRepeatDelay());

					if(e->hasOrientationRange()) {
						glm::quat start, end;
						e->getOrientationRange(&start, &end);
						bool changed = false;
						changed |= QuaternionGui("Orientation Start", &start);
						changed |= QuaternionGui("Orientation End", &end);
						if(changed) {
							e->setOrientationRange(start, end);
						}
						if(ImGui::Button("Remove Orientation Range")) {
							e->clearOrientationRange();
						}
					} else {
						glm::quat q = e->getOrientation();
						if(QuaternionGui("Orientation", &q)) {
							e->setOrientation(q);
						}
						if(ImGui::Button("Add Orientation Range")) {
							e->setOrientationRange(glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f)), glm::angleAxis(2.0f * static_cast<float>(M_PI), glm::vec3(0.0f, 1.0f, 0.0f)));
						}
					}

					//color_range_
					if(e->hasColorRange()) {
						const auto& cr = e->getColorRange();
						bool changed = false;
						float scol[4] = { cr.first.r, cr.first.g, cr.first.b, cr.first.a };
						if(ImGui::ColorEdit4("Start color", scol, true)) {
							changed |= true;
						}
						float ecol[4] = { cr.second.r, cr.second.g, cr.second.b, cr.second.a };
						if(ImGui::ColorEdit4("End color", ecol, true)) {
							changed |= true;
						}

						if(changed) {
							e->setColorRange(glm::vec4(scol[0], scol[1], scol[2], scol[3]), glm::vec4(ecol[0], ecol[1], ecol[2], ecol[3]));
						}

						if(ImGui::Button("Remove Color Range")) {
							e->clearColorRange();
						}
					} else {
						float col[4] = { e->getColorFloat().r, e->getColorFloat().g, e->getColorFloat().b, e->getColorFloat().a };
						if(ImGui::ColorEdit4("color", col, true)) {
							e->setColor(glm::vec4(col[0], col[1], col[2], col[3]));
						}
						if(ImGui::Button("Add Color Range")) {
							e->setColorRange(glm::vec4(0.0f), glm::vec4(1.0f));
						}
					}

					ParameterGui("Width", e->getParticleWidth());
					ParameterGui("Height", e->getParticleHeight());
					ParameterGui("Depth", e->getParticleDepth());

					bool force_emission = e->getForceEmission();
					if(ImGui::Checkbox("Force Emission", &force_emission)) {
						e->setForceEmission(force_emission);
					}
					bool can_be_deleted = e->getCanBeDeleted();
					if(ImGui::Checkbox("Can Be Deleted", &can_be_deleted)) {
						e->setCanBeDeleted(can_be_deleted);
					}

					switch (type) {
					case Particles::EmitterType::POINT: {
						//auto& pe = std::dynamic_pointer_cast<Particles::PointEmitter>(e);
						// no extra UI
						break;
					}
					case KRE::Particles::EmitterType::LINE: {
						auto le = std::dynamic_pointer_cast<Particles::LineEmitter>(e);
						float mini = le->getMinIncrement();
						if(ImGui::DragFloat("Min Increment", &mini, 0.1f, 0.0f, 100.0f)) {
							le->setMinIncrement(mini);
						}
						float maxi = le->getMaxIncrement();
						if(ImGui::DragFloat("Max Increment", &maxi, 0.1f, 0.0f, 100.0f)) {
							le->setMinIncrement(maxi);
						}
						float ld = le->getLineDeviation();
						if(ImGui::DragFloat("Line Deviation", &ld, 0.1f, 0.0f, 100.0f)) {
							le->setLineDeviation(ld);
						}
						break;
					}
					case KRE::Particles::EmitterType::BOX: {
						auto be = std::dynamic_pointer_cast<Particles::BoxEmitter>(e);
						const auto& dims = be->getDimensions();
						float v[3]{ dims.x, dims.y, dims.z };
						if(ImGui::SliderFloat3("Dimensions", v, 0.0f, 100.0f)) {
							be->setDimensions(v);
						}
						break;
					}
					case KRE::Particles::EmitterType::CIRCLE: {
						auto ce = std::dynamic_pointer_cast<Particles::CircleEmitter>(e);
						ParameterGui("Radius", ce->getRadius(), 0.01f, 200.0f);
						float step = ce->getStep();
						if(ImGui::DragFloat("Step", &step, 0.1f, 0.0f, 100.0f)) {
							ce->setStep(step);
						}
						float angle = ce->getAngle();
						if(ImGui::DragFloat("Angle", &angle, 0.1f, 0.0f, 360.0f)) {
							ce->setAngle(angle);
						}
						auto& norm = ce->getNormal();
						float nv[3]{ norm.x, norm.y, norm.z };
						if(ImGui::Button(" XY ")) {
							ce->setNormal(0.0f, 0.0f, 1.0f);
						}
						ImGui::SameLine();
						if(ImGui::Button(" XZ ")) {
							ce->setNormal(0.0f, 1.0f, 0.0f);
						}
						ImGui::SameLine();
						if(ImGui::Button(" YZ ")) {
							ce->setNormal(1.0f, 0.0f, 0.0f);
						}
						if(ImGui::DragFloat3("Normal", nv, 0.05f, 0.0f, 2.0f)) {
							ce->setNormal(nv);
						}

						bool random_loc = ce->isRandomLocation();
						if(ImGui::Checkbox("Random Location", &random_loc)) {
							ce->setRandomLocation(random_loc);
						}
						break;
					}
					case KRE::Particles::EmitterType::SPHERE_SURFACE: {
						auto sse = std::dynamic_pointer_cast<Particles::SphereSurfaceEmitter>(e);
						ParameterGui("Radius", sse->getRadius());
						break;
					}
					default:
						ASSERT_LOG(false, "Unrecognized emitter type: " << static_cast<int>(type));
						break;
					}
				}
			}

			if(emitter_replace) {
				psystem->setEmitter(emitter_replace);
			}

			if(ImGui::CollapsingHeader("Affectors")) {
				std::vector<std::string> affectors;
				std::vector<Particles::AffectorPtr> aff_to_remove;
				// Add button, clear list button
				if(ImGui::SmallButton("Clear All")) {
					aff_to_remove = psystem->getAffectors();
				}
				ImGui::SameLine();
				if(ImGui::SmallButton("Add Affector")) {
					// XXX
					psystem->getAffectors().emplace_back(new Particles::RandomiserAffector(pscontainer));
				}
				for(auto& a : psystem->getAffectors()) {
					std::string aff_name = std::string(Particles::get_affector_name(a->getType())) + " - " + a->getName();
					ImGui::Text("%s", aff_name.c_str());
					ImGui::SameLine();
					ImGui::PushID(a.get());
					if(ImGui::SmallButton("X")) {
						// add to list to remove.
						aff_to_remove.emplace_back(a);
					}
					ImGui::PopID();
				}
				for(auto& aff : aff_to_remove) {
					auto it = std::find(psystem->getAffectors().begin(), psystem->getAffectors().end(), aff);
					if(it != psystem->getAffectors().end()) {
						psystem->getAffectors().erase(it);
					}
				}

				std::vector<std::pair<Particles::AffectorPtr, Particles::AffectorPtr>> affector_replace;
				for(auto& a : psystem->getAffectors()) {
					std::stringstream ss;
					ss << "Affector " << Particles::get_affector_name(a->getType());
					// should have a function to get pretty name of affector here, based on type.
					if(ImGui::CollapsingHeader(ss.str().c_str())) {
						EmitObjectUI(a);

						static std::vector<std::string> ptype{ "Color", "Jet", "Vortex", "Gravity", "Linear Force", "Scale", "Particle Follower", "Align", "Flock Centering", "Black Hole", "Path Follower", "Radomizer", "Sine Force" };
						int current_type = static_cast<int>(a->getType());
						ImGui::PushID(a.get());
						if(ImGui::Combo("Type", &current_type, vector_string_getter, &ptype, ptype.size())) {
							auto new_a = Particles::Affector::factory(pscontainer, static_cast<Particles::AffectorType>(current_type));
							affector_replace.emplace_back(std::make_pair(a, new_a));
						}
						ImGui::PopID();

						//mass
						float mass = a->getMass();
						if(ImGui::SliderFloat("Mass", &mass, 0.0f, 1000.0f)) {
							a->setMass(mass);
						}
						//position
						const auto& pos = a->getPosition();
						float posf[3] = { pos.x, pos.y, pos.z };
						if(ImGui::SliderFloat3("Position", posf, 0.0f, 1000.0f)) {
							a->setPosition(glm::vec3(posf[0], posf[1], posf[2]));
						}
						//scale
						const auto& scale = a->getScale();
						float scalef[3] = { scale.x, scale.y, scale.z };
						if(ImGui::SliderFloat3("Scale", scalef, 0.0f, 1000.0f)) {
							a->setScale(glm::vec3(scalef[0], scalef[1], scalef[2]));
						}

						switch(a->getType()) {
						case Particles::AffectorType::COLOR: {
							auto tca = std::dynamic_pointer_cast<Particles::TimeColorAffector>(a);
							auto op = tca->getOperation();
							int current_item = static_cast<int>(op);
							static std::vector<std::string> optype{ "Set", "Multiply" };
							if(ImGui::Combo("Operation", &current_item, vector_string_getter, &optype, optype.size())) {
								tca->setOperation(static_cast<Particles::TimeColorAffector::ColourOperation>(current_item));
							}					
							auto tcdata = tca->getTimeColorData();
							bool data_changed = false;
							ImGui::BeginGroup();
							if(ImGui::SmallButton("Clear")) {
								tca->clearTimeColorData();
							}
							ImGui::SameLine();
							if(ImGui::SmallButton("+")) {
								tca->addTimecolorEntry(std::make_pair(0.0f, glm::vec4(0.0f)));
							}
							ImGui::EndGroup();

							std::vector<Particles::TimeColorAffector::tc_pair> tc_data_to_remove;
							for(auto& tc : tcdata) {
								ImGui::PushID(&tc.second);
								ImGui::BeginGroup();
								ImGui::PushItemWidth(ImGui::CalcItemWidth() * 0.5f);
								if(ImGui::DragFloat("T", &tc.first, 0.01f, 0.0f, 1.0f)) {
									data_changed = true;
								}
								ImGui::SameLine();
								float col[4] = { tc.second.r,tc.second.g, tc.second.b, tc.second.a };
								if(ImGui::ColorEdit4("C", col)) {
									//if(ColorPicker4(col, true)) {
									tc.second = glm::vec4(col[0], col[1], col[2], col[3]);
									data_changed = true;
								}
								ImGui::PopItemWidth();
								ImGui::SameLine();
								if(ImGui::SmallButton("X")) {
									tc_data_to_remove.emplace_back(tc);
									data_changed = true;
								}
								ImGui::EndGroup();
								ImGui::PopID();								
							}
							for(auto& p : tc_data_to_remove) {
								auto it = std::find(tcdata.begin(), tcdata.end(), p);
								if(it != tcdata.end()) {
									tcdata.erase(it);
									data_changed = true;
								}
							}
							if(data_changed) {
								tca->setTimeColorData(tcdata);
							}
							break;
						}
						case Particles::AffectorType::JET: {
							auto ja = std::dynamic_pointer_cast<Particles::JetAffector>(a);
							ParameterGui("acceleration", ja->getAcceleration(), 0.0f, 100.0f);
							break;
						}
						case Particles::AffectorType::VORTEX: {
							auto va = std::dynamic_pointer_cast<Particles::VortexAffector>(a);
							// rotation axis
							ImGui::BeginGroup();
							if(ImGui::Button(" +X ")) {
								va->setRotationAxis(glm::vec3(1.0f, 0.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" +Y ")) {
								va->setRotationAxis(glm::vec3(0.0f, 1.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" +Z ")) {
								va->setRotationAxis(glm::vec3(0.0f, 0.0f, 1.0f));
							}
							if(ImGui::Button(" -X ")) {
								va->setRotationAxis(glm::vec3(-1.0f, 0.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" -Y ")) {
								va->setRotationAxis(glm::vec3(0.0f, -1.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" -Z ")) {
								va->setRotationAxis(glm::vec3(0.0f, 0.0f, -1.0f));
							}
							const auto& axis = va->getRotationAxis();
							float v[3] { axis.x, axis.y, axis.z };
							if(ImGui::SliderFloat3("Rotation Axis", v, -1.0f, 1.0f)) {
								va->setRotationAxis(glm::vec3(v[0], v[1], v[2]));
							}
							ImGui::EndGroup();
							// rotation speed
							ParameterGui("Rotation Speed", va->getRotationSpeed());
							break;
						}
						case Particles::AffectorType::GRAVITY: {
							auto ga = std::dynamic_pointer_cast<Particles::GravityAffector>(a);
							ParameterGui("Gravity", ga->getGravity());
							break;
						}
						case Particles::AffectorType::LINEAR_FORCE: {
							auto fa = std::dynamic_pointer_cast<Particles::LinearForceAffector>(a);
							ParameterGui("Force", fa->getForce());

							// rdirection
							ImGui::BeginGroup();
							if(ImGui::Button(" +X ")) {
								fa->setDirection(glm::vec3(1.0f, 0.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" +Y ")) {
								fa->setDirection(glm::vec3(0.0f, 1.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" +Z ")) {
								fa->setDirection(glm::vec3(0.0f, 0.0f, 1.0f));
							}
							if(ImGui::Button(" -X ")) {
								fa->setDirection(glm::vec3(-1.0f, 0.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" -Y ")) {
								fa->setDirection(glm::vec3(0.0f, -1.0f, 0.0f));
							}
							ImGui::SameLine();
							if(ImGui::Button(" -Z ")) {
								fa->setDirection(glm::vec3(0.0f, 0.0f, -1.0f));
							}

							const auto& dir = fa->getDirection();
							float v[3]{ dir[0], dir[1], dir[2] };
							if(ImGui::SliderFloat3("Direction", v, -1.0f, 1.0f)) {
								fa->setDirection(glm::vec3(v[0], v[1], v[2]));
							}
							ImGui::EndGroup();
							break;
						}
						case Particles::AffectorType::SCALE: {
							auto sa = std::dynamic_pointer_cast<Particles::ScaleAffector>(a);
							auto& xyz_scale = sa->getScaleXYZ();
							auto& x_scale = sa->getScaleX();
							auto& y_scale = sa->getScaleY();
							auto& z_scale = sa->getScaleZ();
							// XXX need some way to deal with locked scales as opposed to
							// independent scales.
							if(xyz_scale) {
								ParameterGui("XYZ Scale", xyz_scale, 0.0f, 100.0f);
							} else {
								if(x_scale) {
									ParameterGui("X Scale", x_scale, 0.0f, 100.0f);
								}
								if(y_scale) {
									ParameterGui("Y Scale", y_scale, 0.0f, 100.0f);
								}
								if(z_scale) {
									ParameterGui("Z Scale", z_scale, 0.0f, 100.0f);
								}
							}
							break;
						}
						case Particles::AffectorType::PARTICLE_FOLLOWER: {
							auto pfa = std::dynamic_pointer_cast<Particles::ParticleFollowerAffector>(a);
							float minmaxd[2]{ pfa->getMinDistance(), pfa->getMaxDistance() };
							if(ImGui::DragFloat2("Min/Max Distance", minmaxd, 1.0f, 0.0f, 1000.0f)) {
								pfa->setMinDistance(minmaxd[0]);
								pfa->setMaxDistance(minmaxd[1]);
							}
							break;
						}
						case Particles::AffectorType::ALIGN: {
							auto aa = std::dynamic_pointer_cast<Particles::AlignAffector>(a);
							bool resize = aa->getResizeable();
							if(ImGui::Checkbox("Resize", &resize)) {
								aa->setResizeable(resize);
							}
							break;
						}
						case Particles::AffectorType::FLOCK_CENTERING: {
							// no extra UI components.
							break;
						}
						case Particles::AffectorType::BLACK_HOLE: {
							auto bha = std::dynamic_pointer_cast<Particles::BlackHoleAffector>(a);
							ParameterGui("Velocity", bha->getVelocity());
							ParameterGui("Acceleration", bha->getAcceleration());
							break;
						}
						case Particles::AffectorType::PATH_FOLLOWER: {
							// XXX todo
							break;
						}
						case Particles::AffectorType::RANDOMISER: {
							auto ra = std::dynamic_pointer_cast<Particles::RandomiserAffector>(a);
							float ts = ra->getTimeStep();
							if(ImGui::DragFloat("Time Step", &ts, 1.0f, 0.0f, 10.0f)) {
								ra->setTimeStep(ts);
							}

							bool random_direction = ra->isRandomDirection();
							if(ImGui::Checkbox("Random Direction", &random_direction)) {
								ra->setRandomDirection(random_direction);
							}

							auto& max_deviation = ra->getDeviation();
							float v[3]{ max_deviation.x, max_deviation.y, max_deviation.z };
							if(ImGui::DragFloat3("Max Deviation", v, 0.1f, 0.0f, 100.0f)) {
								ra->setDeviation(v[0], v[1], v[2]);
							}
							break;
						}
						case Particles::AffectorType::SINE_FORCE: {
							auto sfa = std::dynamic_pointer_cast<Particles::SineForceAffector>(a);

							float v[2]{ sfa->getMinFrequency(), sfa->getMaxFrequency() };
							if(ImGui::DragFloat2("Min/Max Frequency", v, 0.1f, 0.0f, 1000.0f)) {
								sfa->setMinFrequency(v[0]);
								sfa->setMaxFrequency(v[1]);
							}

							auto& force_vector = sfa->getForceVector();
							float fv[3]{ force_vector.x, force_vector.y, force_vector.z };
							if(ImGui::DragFloat3("Force Vector", fv, 0.1f, 0.0f, 1000.0f)) {
								sfa->setForceVector(fv[0], fv[1], fv[2]);
							}
							auto op = sfa->getForceApplication();
							int current_item = static_cast<int>(op);
							const char* const optype[] = { "Add", "Average" };
							if(ImGui::Combo("Force Application", &current_item, optype, 2)) {
								sfa->setForceApplication(static_cast<Particles::SineForceAffector::ForceApplication>(current_item));
							}			
							break;
						}
						default:
							ASSERT_LOG(false, "Unhandled affector type: " << static_cast<int>(a->getType()));
							break;
						}
						//excluded emitters
					}
				}
				for(auto& aff : affector_replace) {
					auto it = std::find(psystem->getAffectors().begin(), psystem->getAffectors().end(), aff.first);
					if(it != psystem->getAffectors().end()) {
						psystem->getAffectors().erase(it);
					}
					psystem->getAffectors().emplace_back(aff.second);
				}
			}
			ImGui::End();
		}

	}
}
