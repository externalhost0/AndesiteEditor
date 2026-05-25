//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include "ImNodeFlow.h"

namespace Andesite {
	// NODE STYLING //
	static constexpr float kGlobalBorderRadius = 3.5f;
	static constexpr ImColor kGlobalTitleColor = {233, 241, 244, 255};

	inline std::shared_ptr<ImFlow::NodeStyle> baseStyle(ImU32 color) {
		auto s = std::make_shared<ImFlow::NodeStyle>(color, kGlobalTitleColor, kGlobalBorderRadius);
		s->padding = ImVec4(16, 6, 16, 2);
		s->border_selected_thickness = 2.5f;
		s->border_selected_color = IM_COL32(218, 226, 232, 250);
		return s;
	}

	static std::shared_ptr<ImFlow::NodeStyle> inputStyle() { return baseStyle(IM_COL32(220, 53, 69, 255)); }
	static std::shared_ptr<ImFlow::NodeStyle> convertStyle() { return baseStyle(IM_COL32(199, 156, 26, 255)); }
	static std::shared_ptr<ImFlow::NodeStyle> textureStyle() { return baseStyle(IM_COL32(207, 133, 23, 255)); }
	static std::shared_ptr<ImFlow::NodeStyle> shaderStyle() { return baseStyle(IM_COL32(76, 175, 80, 255)); }
	static std::shared_ptr<ImFlow::NodeStyle> singleStyle() { return baseStyle(IM_COL32(158, 158, 158, 255)); }

	// PIN STYLING //
	static constexpr float kDefualtPinSize = 4.f;
	static constexpr float kPinHoverSize = kDefualtPinSize + 0.5f;
	static std::shared_ptr<ImFlow::PinStyle> scalarPinStyle() { return std::make_shared<ImFlow::PinStyle>(ImFlow::PinStyle(IM_COL32(128, 128, 128, 255), 0, kDefualtPinSize, kPinHoverSize, kDefualtPinSize, 1.f)); }
	static std::shared_ptr<ImFlow::PinStyle> vectorPinStyle() { return std::make_shared<ImFlow::PinStyle>(ImFlow::PinStyle(IM_COL32(146, 40, 189, 255), 0, kDefualtPinSize, kPinHoverSize, kDefualtPinSize, 1.f)); }
	static std::shared_ptr<ImFlow::PinStyle> colorPinStyle() { return std::make_shared<ImFlow::PinStyle>(ImFlow::PinStyle(IM_COL32(219, 204, 39, 255), 0, kDefualtPinSize, kPinHoverSize, kDefualtPinSize, 1.f)); }
	static std::shared_ptr<ImFlow::PinStyle> shaderPinStyle() { return std::make_shared<ImFlow::PinStyle>(ImFlow::PinStyle(IM_COL32(62, 168, 50, 255), 0, kDefualtPinSize, kPinHoverSize, kDefualtPinSize, 1.f)); }
}