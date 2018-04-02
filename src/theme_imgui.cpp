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

#include "theme_imgui.hpp"

#ifdef USE_IMGUI

#include <string>

#include "asserts.hpp"
#include "imgui.h"
#include "json_parser.hpp"
#include "filesystem.hpp"
#include "preferences.hpp"
#include "variant_utils.hpp"

namespace
{
	const std::string imgui_theme_file = "data/imgui.cfg"; 
}

// A default theme
void theme_imgui_default()
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

	load_imgui_theme();
}


void ImVec4ToFloat4(float* v, const ImVec4& cvec)
{
	v[0] = cvec.x;
	v[1] = cvec.y;
	v[2] = cvec.z;
	v[3] = cvec.w;
}

void Float2ToImVec2(ImVec2* cvec, float* v)
{
	cvec->x = v[0];
	cvec->y = v[1];
}

void ImVec2ToFloat2(float* v, const ImVec2& cvec)
{
	v[0] = cvec.x;
	v[1] = cvec.y;
}

void Float4ToImVec4(ImVec4* cvec, float* v)
{
	cvec->x = v[0];
	cvec->y = v[1];
	cvec->z = v[2];
	cvec->w = v[3];
}

ImVec2 variant_to_vec2(const variant& v)
{
	ImVec2 res;
	ASSERT_LOG(v.is_list() && v.num_elements() == 2, "Value is not a list of 2 elements.");
	res.x = v[0].as_float();
	res.y = v[1].as_float();
	return res;
}

variant vec2_to_variant(const ImVec2& v)
{
	std::vector<variant> res;
	res.emplace_back(variant(v.x));
	res.emplace_back(variant(v.y));
	return variant(&res);
}

void EditColor(const char* label, ImVec4* col)
{
	float v[4];
	ImVec4ToFloat4(v, *col);
	if(ImGui::ColorEdit4(label, v)) {
		Float4ToImVec4(col, v);
	}
}

void EditVec2(const char* label, ImVec2* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f)
{
	float f[2];
	ImVec2ToFloat2(f, *v);
	if(ImGui::DragFloat2(label, f, v_speed, v_min, v_max)) {
		Float2ToImVec2(v, f);
	}
}

void imgui_theme_ui()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::Begin("ImGui Theme Editor");
	if(ImGui::Button("Save theme")) {
		save_imgui_theme();
	}
	if(ImGui::CollapsingHeader("Colors")) {
		EditColor("Text", &style.Colors[ImGuiCol_Text]);
		EditColor("TextDisabled", &style.Colors[ImGuiCol_TextDisabled]);
		EditColor("Window Background", &style.Colors[ImGuiCol_WindowBg]);
		EditColor("Menu Bar Background", &style.Colors[ImGuiCol_MenuBarBg]);
		EditColor("Child Window Background", &style.Colors[ImGuiCol_ChildWindowBg]);
		EditColor("Border", &style.Colors[ImGuiCol_Border]);
		EditColor("Border Shadow", &style.Colors[ImGuiCol_BorderShadow]);
		EditColor("Frame Background", &style.Colors[ImGuiCol_FrameBg]);
		EditColor("Frame Background Hovered", &style.Colors[ImGuiCol_FrameBgHovered]);
		EditColor("Frame Background Active", &style.Colors[ImGuiCol_FrameBgActive]);
		EditColor("Title Background", &style.Colors[ImGuiCol_TitleBg]);
		EditColor("Title Bsackground Collapsed", &style.Colors[ImGuiCol_TitleBgCollapsed]);
		EditColor("Scrollbar Background", &style.Colors[ImGuiCol_ScrollbarBg]);
		EditColor("Scrollbar Grab", &style.Colors[ImGuiCol_ScrollbarGrab]);
		EditColor("Scrollbar Grab Hovered", &style.Colors[ImGuiCol_ScrollbarGrabHovered]);
		EditColor("Scrollbar Grab Active", &style.Colors[ImGuiCol_ScrollbarGrabActive]);
		EditColor("Combo Background", &style.Colors[ImGuiCol_ComboBg]);
		EditColor("Slider Grab Active", &style.Colors[ImGuiCol_SliderGrabActive]);
		EditColor("Button", &style.Colors[ImGuiCol_Button]);
		EditColor("Button Hovered", &style.Colors[ImGuiCol_ButtonHovered]);
		EditColor("Button Active", &style.Colors[ImGuiCol_ButtonActive]);
		EditColor("Header", &style.Colors[ImGuiCol_Header]);
		EditColor("Header Hovered", &style.Colors[ImGuiCol_HeaderHovered]);
		EditColor("Header Active", &style.Colors[ImGuiCol_HeaderActive]);
		EditColor("Resize Grip", &style.Colors[ImGuiCol_ResizeGrip]);
		EditColor("Resize Grip Hovered", &style.Colors[ImGuiCol_ResizeGripHovered]);
		EditColor("Resize Grip Active", &style.Colors[ImGuiCol_ResizeGripActive]);
		EditColor("Close Button", &style.Colors[ImGuiCol_CloseButton]);
		EditColor("Close Button Hovered", &style.Colors[ImGuiCol_CloseButtonHovered]);
		EditColor("Close Button Active", &style.Colors[ImGuiCol_CloseButtonActive]);
		EditColor("Text Selected Background", &style.Colors[ImGuiCol_TextSelectedBg]);
		EditColor("Modal Window Darkening", &style.Colors[ImGuiCol_ModalWindowDarkening]);
	}
	if(ImGui::CollapsingHeader("Styles")) {
		ImGui::SliderFloat("Global Opacity", &style.Alpha, 0.2f, 1.0f);
		EditVec2("Window Padding", &style.WindowPadding, 0.2f, 0.0f, 20.0f);
		EditVec2("Window Min. Size", &style.WindowMinSize, 1.0f, 0.0f, 200.0f);
		ImGui::DragFloat("Window Rounding", &style.WindowRounding, 0.1f, 0.0f, 10.0f);
		EditVec2("Window Title Align", &style.WindowTitleAlign, 0.1f, 0.0f, 1.0f);
		ImGui::DragFloat("Child Window Rounding", &style.ChildWindowRounding, 0.05f, 0.0f, 10.0f);
		EditVec2("Frame Padding", &style.FramePadding, 0.2f, 0.0f, 20.0f);
		ImGui::DragFloat("Frame Rounding", &style.FrameRounding, 0.1f, 0.0f, 10.0f);
		EditVec2("Item Spacing", &style.ItemSpacing, 0.05f, 0.0f, 10.0f);
		EditVec2("Item Inner Spacing", &style.ItemInnerSpacing, 0.05f, 0.0f, 10.0f);
		EditVec2("Touch Extra Padding", &style.TouchExtraPadding, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("Indent Spacing", &style.IndentSpacing, 1.0f, 0.0f, 100.0f);
		ImGui::DragFloat("Column Min. Spacing", &style.ColumnsMinSpacing, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("Scrollbar Size", &style.ScrollbarSize, 0.2f, 0.0f, 20.0f);
		ImGui::DragFloat("Scrollbar Rounding", &style.ScrollbarRounding, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("Grab Min Size", &style.GrabMinSize, 0.5f, 0.0f, 100.0f);
		ImGui::DragFloat("Grab Rounding", &style.GrabRounding, 0.05f, 0.0f, 10.0f);
		EditVec2("Button Text Align", &style.ButtonTextAlign, 0.1f, 0.0f, 1.0f);
		EditVec2("Display Window Padding", &style.DisplayWindowPadding, 1.0f, 0.0f, 100.0f);
		EditVec2("Display Safe Area", &style.DisplaySafeAreaPadding, 0.5f, 0.0f, 100.0f);
		ImGui::Checkbox("Anti-Aliased Lines", &style.AntiAliasedLines);
		ImGui::Checkbox("Anti-Aliased Shapes", &style.AntiAliasedShapes);
		ImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.1f, 0.0f, 100.0f);
	}
	ImGui::End();
}

void save_imgui_theme()
{
	ImGuiStyle& style = ImGui::GetStyle();
	std::string fname = std::string(preferences::user_data_path()) + imgui_theme_file;
	variant_builder res;
	res.add("alpha", style.Alpha);
	res.add("window_padding", vec2_to_variant(style.WindowPadding));
	res.add("window_rounding", style.WindowRounding);
	res.add("window_min_size", vec2_to_variant(style.WindowMinSize));
	res.add("child_window_rounding", style.ChildWindowRounding);
	res.add("window_title_align", vec2_to_variant(style.WindowTitleAlign));
	res.add("frame_padding", vec2_to_variant(style.FramePadding));
	res.add("frame_rounding", style.FrameRounding);
	res.add("item_spacing", vec2_to_variant(style.ItemSpacing));
	res.add("item_inner_spacing", vec2_to_variant(style.ItemInnerSpacing));
	res.add("touch_extra_padding", vec2_to_variant(style.TouchExtraPadding));
	res.add("indent_spacing", style.IndentSpacing);
	res.add("columns_min_spacing", style.ColumnsMinSpacing);
	res.add("scrollbar_size", style.ScrollbarSize);
	res.add("scrollbar_rounding", style.ScrollbarRounding);
	res.add("grab_min_size", style.GrabMinSize);
	res.add("grab_rounding", style.GrabRounding);
	res.add("button_text_align", vec2_to_variant(style.ButtonTextAlign));
	res.add("display_window_padding", vec2_to_variant(style.DisplayWindowPadding));
	res.add("display_safe_area", vec2_to_variant(style.DisplaySafeAreaPadding));
	res.add("anti_aliased_lines", style.AntiAliasedLines);
	res.add("anti_aliased_shapes", style.AntiAliasedShapes);
	res.add("curve_tessellation_tolerance", style.CurveTessellationTol);
	sys::write_file(fname, res.build().write_json());
}

void load_theme_from_variant(const variant& v)
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Alpha = v["alpha"].as_float(style.Alpha);
	if(v.has_key("window_padding")) {
		style.WindowPadding = variant_to_vec2(v["window_padding"]);
	}
	if(v.has_key("window_min_size")) {
		style.WindowMinSize = variant_to_vec2(v["window_min_size"]);
	}
	style.WindowRounding = v["window_rounding"].as_float(style.WindowRounding);
	if(v.has_key("window_title_align")) {
		style.WindowTitleAlign = variant_to_vec2(v["window_title_align"]);
	}
	style.ChildWindowRounding = v["child_window_rounding"].as_float(style.ChildWindowRounding);
	if(v.has_key("frame_padding")) {
		style.FramePadding = variant_to_vec2(v["frame_padding"]);
	}
	style.FrameRounding = v["frame_rounding"].as_float(style.FrameRounding);
	if(v.has_key("item_spacing")) {
		style.ItemSpacing = variant_to_vec2(v["item_spacing"]);
	}
	if(v.has_key("item_inner_spacing")) {
		style.ItemInnerSpacing = variant_to_vec2(v["item_inner_spacing"]);
	}
	if(v.has_key("touch_extra_padding")) {
		style.TouchExtraPadding = variant_to_vec2(v["touch_extra_padding"]);
	}
	style.IndentSpacing = v["indent_spacing"].as_float(style.IndentSpacing);
	style.ColumnsMinSpacing = v["columns_min_spacing"].as_float(style.ColumnsMinSpacing);
	style.ScrollbarSize = v["scrollbar_size"].as_float(style.ScrollbarSize);
	style.ScrollbarRounding = v["scrollbar_rounding"].as_float(style.ScrollbarRounding);
	style.GrabMinSize = v["grab_min_size"].as_float(style.GrabMinSize);
	style.GrabRounding = v["grab_rounding"].as_float(style.GrabRounding);
	if(v.has_key("button_text_align")) {
		style.ButtonTextAlign = variant_to_vec2(v["button_text_align"]);
	}
	if(v.has_key("display_window_padding")) {
		style.DisplayWindowPadding = variant_to_vec2(v["display_window_padding"]);
	}
	if(v.has_key("display_safe_area")) {
		style.DisplaySafeAreaPadding = variant_to_vec2(v["display_safe_area"]);
	}
	style.AntiAliasedLines = v["anti_aliased_lines"].as_bool(style.AntiAliasedLines);
	style.AntiAliasedShapes = v["anti_aliased_shapes"].as_bool(style.AntiAliasedShapes);
	style.CurveTessellationTol = v["curve_tessellation_tolerance"].as_float(style.CurveTessellationTol);
}

void load_imgui_theme()
{
	std::string fname = std::string(preferences::user_data_path()) + imgui_theme_file;
	try {
		variant v = json::parse_from_file(fname);
		load_theme_from_variant(v);
	} catch(json::ParseError& e) {
		LOG_INFO("Failed to read file: " << fname << " : " << e.errorMessage());
	}
}

#else
void theme_imgui_default()
{
}

void imgui_theme_ui()
{
}

void save_imgui_theme()
{
}

void load_imgui_theme()
{
}
#endif
