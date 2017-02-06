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

#include "imgui.h"

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

void EditColor(char* label, ImVec4* col)
{
	float v[4];
	ImVec4ToFloat4(v, *col);
	if(ImGui::ColorEdit4(label, v)) {
		Float4ToImVec4(col, v);
	}
}

void EditVec2(char* label, ImVec2* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f)
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

#else
void theme_imgui_default()
{
}

void imgui_theme_ui()
{
}
#endif
