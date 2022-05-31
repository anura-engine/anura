/*
	Example of use:

	ImVec2 foo[10];
	...
	foo[0].x = -1; // init data so editor knows to take it from here
	...
	if (ImGui::Curve("Das editor", ImVec2(600, 200), 10, foo))
	{
		// curve changed
	}
	...
	float value_you_care_about = ImGui::CurveValue(0.7f, 10, foo); // calculate value at position 0.7
*/

#include "imgui_custom.h"
#include "spline_simple.h"

#include <cmath>
#include <imgui_internal.h>
#include <iostream>

namespace ImGui
{

	float CurveValue(float p, int maxpoints, const ImVec2 *points)
	{
		if (maxpoints < 2 || points == 0)
			return 0;
		if (p < 0) return points[0].y;

		int left = 0;
		while (left < maxpoints && points[left].x < p && points[left].x != -1) left++;
		if (left) left--;

		if (left == maxpoints-1)
			return points[maxpoints - 1].y;

		float d = (p - points[left].x) / (points[left + 1].x - points[left].x);

		return points[left].y + (points[left + 1].y - points[left].y) * d;
	}

	int Curve(const char *label, const ImVec2& size, int maxpoints, ImVec2 *points)
	{
		int modified = 0;
		int i;
		if (maxpoints < 2 || points == 0)
			return 0;

		if (points[0].x < 0)
		{
			points[0].x = 0;
			points[0].y = 0;
			points[1].x = 1;
			points[1].y = 1;
			points[2].x = -1;
		}

		ImGuiWindow* window = GetCurrentWindow();
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImGuiID id = window->GetID(label);
		if (window->SkipItems)
			return 0;

		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
		ItemSize(bb);
		if (!ItemAdd(bb, 0))
			return 0;

		const bool hovered = IsItemHovered(ImGuiHoveredFlags_None);

		int max = 0;
		while (max < maxpoints && points[max].x >= 0) max++;

		int kill = 0;
		do
		{
			if (kill)
			{
				modified = 1;
				for (i = kill + 1; i < max; i++)
				{
					points[i - 1] = points[i];
				}
				max--;
				points[max].x = -1;
				kill = 0;
			}

			for (i = 1; i < max - 1; i++)
			{
				if (fabs(points[i].x - points[i - 1].x) < 1.0f / 128.0f)
				{
					kill = i;
				}
			}
		}
		while (kill);


		RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

		float ht = bb.Max.y - bb.Min.y;
		float wd = bb.Max.x - bb.Min.x;

		if (hovered)
		{
			SetHoveredID(id);

			ImGuiIO& io = GetIO();
			if (io.MouseDown[0])
			{
				modified = 1;
				ImVec2 pos = (io.MousePos - bb.Min) / (bb.Max - bb.Min);
				pos.y = 1 - pos.y;

				int left = 0;
				while (left < max && points[left].x < pos.x) left++;
				if (left) left--;

				ImVec2 p = points[left] - pos;
				float p1d = std::sqrt(p.x*p.x + p.y*p.y);
				p = points[left+1] - pos;
				float p2d = std::sqrt(p.x*p.x + p.y*p.y);
				int sel = -1;
				if (p1d < (1.f / 16.0f)) sel = left;
				if (p2d < (1.f / 16.0f)) sel = left + 1;

				if (sel != -1)
				{
					points[sel] = pos;
				}
				else
				{
					if (max < maxpoints)
					{
						max++;
						for (i = max; i > left; i--)
						{
							points[i] = points[i - 1];
						}
						points[left + 1] = pos;
					}
					if (max < maxpoints)
						points[max].x = -1;
				}


				// snap first/last to min/max
				points[0].x = 0;
				points[max - 1].x = 1;
			}
		}

		// bg grid
		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 2),
			ImVec2(bb.Max.x, bb.Min.y + ht / 2),
			GetColorU32(ImGuiCol_TextDisabled), 3);

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4),
			GetColorU32(ImGuiCol_TextDisabled));

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4 * 3),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4 * 3),
			GetColorU32(ImGuiCol_TextDisabled));

		for (i = 0; i < 9; i++)
		{
			window->DrawList->AddLine(
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Min.y),
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Max.y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		// lines
		for (i = 1; i < max; i++)
		{
			ImVec2 a = points[i - 1];
			ImVec2 b = points[i];
			a.y = 1 - a.y;
			b.y = 1 - b.y;
			a = a * (bb.Max - bb.Min) + bb.Min;
			b = b * (bb.Max - bb.Min) + bb.Min;
			window->DrawList->AddLine(a, b, GetColorU32(ImGuiCol_PlotLines));
		}

		if (hovered)
		{
			// control points
			for (i = 0; i < max; i++)
			{
				ImVec2 p = points[i];
				p.y = 1 - p.y;
				p = p * (bb.Max - bb.Min) + bb.Min;
				ImVec2 a = p - ImVec2(2, 2);
				ImVec2 b = p + ImVec2(2, 2);
				window->DrawList->AddRect(a, b, GetColorU32(ImGuiCol_PlotLines));
			}
		}

		RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y + style.FramePadding.y), bb.Max, label, NULL, NULL, ImVec2(0.5f, 0.0f));
		return modified;
	}

}

namespace ImGui
{
	bool Spline(const char *label, const ImVec2& size, int maxpoints, ImVec2 *points)
	{
		bool modified = false;
		int i;
		if (maxpoints < 2 || points == 0)
			return 0;

		if (points[0].x < 0)
		{
			points[0].x = 0;
			points[0].y = 0;
			points[1].x = 1;
			points[1].y = 1;
			points[2].x = -1;
		}

		ImGuiWindow* window = GetCurrentWindow();
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImGuiID id = window->GetID(label);
		if (window->SkipItems) {
			return 0;
		}

		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
		ItemSize(bb);
		if (!ItemAdd(bb, 0)) {
			return 0;
		}

		const bool hovered = IsItemHovered(ImGuiHoveredFlags_None);

		int max = 0;
		while (max < maxpoints && points[max].x >= 0) {
			max++;
		}

		int kill = 0;
		do {
			if(kill) {
				modified = true;
				for(int i = kill + 1; i < max; i++) {
					points[i - 1] = points[i];
				}
				--max;
				points[max].x = -1;
				kill = 0;
			}

			for(int i = 1; i < max - 1; i++) {
				if(fabs(points[i].x - points[i - 1].x) < 1.0f / 128.0f) {
					kill = i;
				}
			}
		} while(kill);


		RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

		float ht = bb.Max.y - bb.Min.y;
		float wd = bb.Max.x - bb.Min.x;

		if(hovered) {
			SetHoveredID(id);

			ImGuiIO& io = GetIO();
			if(io.MouseDown[0]) {
				modified = 1;
				ImVec2 pos = (io.MousePos - bb.Min) / (bb.Max - bb.Min);
				pos.y = 1 - pos.y;

				int left = 0;
				while (left < max && points[left].x < pos.x) left++;
				if (left) left--;

				ImVec2 p = points[left] - pos;
				float p1d = std::sqrt(p.x*p.x + p.y*p.y);
				p = points[left+1] - pos;
				float p2d = std::sqrt(p.x*p.x + p.y*p.y);
				int sel = -1;
				if (p1d < (1.f / 16.0f)) {
					sel = left;
				}
				if (p2d < (1.f / 16.0f)) {
					sel = left + 1;
				}

				if(sel != -1) {
					points[sel] = pos;
				} else {
					if(max < maxpoints) {
						max++;
						for(i = max; i > left; i--) {
							points[i] = points[i - 1];
						}
						points[left + 1] = pos;
					}
					if(max < maxpoints) {
						points[max].x = -1;
					}
				}


				// snap first/last to min/max
				points[0].x = 0;
				points[max - 1].x = 1;
			}
		}

		// bg grid
		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 2),
			ImVec2(bb.Max.x, bb.Min.y + ht / 2),
			GetColorU32(ImGuiCol_TextDisabled), 3);

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4),
			GetColorU32(ImGuiCol_TextDisabled));

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4 * 3),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4 * 3),
			GetColorU32(ImGuiCol_TextDisabled));

		for(int i = 0; i < 9; i++) {
			window->DrawList->AddLine(
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Min.y),
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Max.y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		// lines
		ImVector<float> zpp;
		zpp.resize(max);
		geometry::spline(points, max, zpp.Data);

		float nn = wd;
		for(int i = 1; i < nn; i++) {

			ImVec2 a = ImVec2((i-1) / nn, geometry::interpolate((i-1) / nn, points, max, zpp.Data));
			ImVec2 b = ImVec2(i / nn, geometry::interpolate(i / nn, points, max, zpp.Data));

			a.y = 1 - a.y;
			b.y = 1 - b.y;
			a = a * (bb.Max - bb.Min) + bb.Min;
			b = b * (bb.Max - bb.Min) + bb.Min;
			window->DrawList->AddLine(a, b, GetColorU32(ImGuiCol_PlotLines));
		}

		if(hovered)	{
			// control points
			for(int i = 0; i < max; i++) {
				ImVec2 p = points[i];
				p.y = 1 - p.y;
				p = p * (bb.Max - bb.Min) + bb.Min;
				ImVec2 a = p - ImVec2(2.0f, 2.0f);
				ImVec2 b = p + ImVec2(2.0f, 2.0f);
				window->DrawList->AddRect(a, b, GetColorU32(ImGuiCol_PlotLines));
			}
		}

		RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y + style.FramePadding.y), bb.Max, label, NULL, NULL, ImVec2(0.5f, 0.0f));
		return modified;
	}
}
