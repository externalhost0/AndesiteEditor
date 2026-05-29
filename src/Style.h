//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include <array>
#include <cmath>

#include "ImNodeFlow.h"

#include "Types.h"

namespace Andesite {
	// just alias it cause we type it so much
	using NodeStyle = std::shared_ptr<ImFlow::NodeStyle>;
	using PinStyle = std::shared_ptr<ImFlow::PinStyle>;

	// NODE STYLING //
	static constexpr float kGlobalBorderRadius = 3.5f;
	static constexpr ImColor kGlobalTitleColor = {233, 241, 244, 255};

	inline std::shared_ptr<ImFlow::NodeStyle> baseNodeStyle(ImU32 color) {
		auto s = std::make_shared<ImFlow::NodeStyle>(color, kGlobalTitleColor, kGlobalBorderRadius);
		s->padding = ImVec4(16, 6, 16, 2);
		s->border_selected_thickness = 2.0f;
		s->border_selected_color = IM_COL32(130, 180, 250, 250);
		return s;
	}

	static NodeStyle inputStyle()   { return baseNodeStyle(IM_COL32(220, 53, 69, 255)); }
	static NodeStyle convertStyle() { return baseNodeStyle(IM_COL32(199, 156, 26, 255)); }
	static NodeStyle textureStyle() { return baseNodeStyle(IM_COL32(207, 133, 23, 255)); }
	static NodeStyle shaderStyle()  { return baseNodeStyle(IM_COL32(76, 175, 80, 255)); }
	static NodeStyle uniqueStyle()  { return baseNodeStyle(IM_COL32(158, 158, 158, 255)); }

	// PIN STYLING //
	static constexpr float kDefualtPinSize = 4.f;
	static constexpr float kPinHoverSize = kDefualtPinSize + 0.5f;
	static constexpr ImU32 kScalarPinColor = IM_COL32(128, 128, 128, 255);
	static constexpr ImU32 kVec2PinColor = IM_COL32(61, 210, 125, 255);
	static constexpr ImU32 kVec3PinColor = IM_COL32(77, 142, 234, 255);
	static constexpr ImU32 kVec4PinColor = IM_COL32(149, 68, 226, 255);
	static constexpr ImU32 kShaderPinColor = IM_COL32(198, 242, 78, 255);

	inline std::shared_ptr<ImFlow::PinStyle> basePinStyle(ImU32 color) {
		auto s = std::make_shared<ImFlow::PinStyle>(ImFlow::PinStyle(color, 0, kDefualtPinSize, kPinHoverSize, kDefualtPinSize, 1.f));
		return s;
	}

	// gray
	static PinStyle scalarPinStyle() { return basePinStyle(kScalarPinColor); }
	// green
	static PinStyle vec2PinStyle() { return basePinStyle(kVec2PinColor); }
	// yellow
	static PinStyle vec3PinStyle() { return basePinStyle(kVec3PinColor); }
	// purple
	static PinStyle vec4PinStyle()  { return basePinStyle(kVec4PinColor); }
	// lime
	static PinStyle shaderPinStyle() { return basePinStyle(kShaderPinColor); }

	// helpers for our animation
	inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
		const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
		const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
		return ImGui::ColorConvertFloat4ToU32({
			ca.x + (cb.x - ca.x) * t,
			ca.y + (cb.y - ca.y) * t,
			ca.z + (cb.z - ca.z) * t,
			ca.w + (cb.w - ca.w) * t,
		});
	}

	inline ImU32 AdaptivePinColor() {
		// the colors we want to cycle through
		static constexpr std::array colors = {
			kScalarPinColor,
			kVec2PinColor,
			kVec3PinColor,
			kVec4PinColor,
		};
		const double phase = ImGui::GetTime() * 1.5;
		const auto base = static_cast<size_t>(std::floor(phase)) % colors.size();
		const auto next = (base + 1) % colors.size();
		const auto t = static_cast<float>(phase - std::floor(phase));
		return LerpColor(colors[base], colors[next], t);
	}

	inline ImU32 ConnectionTypeColor(ConnectionType connectionType) {
		switch (connectionType) {
			case ConnectionType::Float: return kScalarPinColor;
			case ConnectionType::Float2: return kVec2PinColor;
			case ConnectionType::Float3: return kVec3PinColor;
			case ConnectionType::Float4: return kVec4PinColor;
			case ConnectionType::Adaptive: return AdaptivePinColor();
		}
		assert(false && "Unhandled case in ConnectionTypeColor");
	}

	// cycles between all colors
	static PinStyle adaptivePinStyle() { return basePinStyle(AdaptivePinColor()); }

	static PinStyle ConnectionTypeToStyle(ConnectionType connectionType) {
		switch (connectionType) {
			case ConnectionType::Float: return scalarPinStyle();
			case ConnectionType::Float2: return vec2PinStyle();
			case ConnectionType::Float3: return vec3PinStyle();
			case ConnectionType::Float4: return vec4PinStyle();
			case ConnectionType::Adaptive: return adaptivePinStyle();
		}
		assert(false && "Unhandled case in ConnectionTypeToStyle");
	}
}
