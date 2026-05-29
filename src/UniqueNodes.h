//
// Created by Hayden Rivas on 5/28/26.
//

#pragma once

#include "NodeBasics.h"

namespace Andesite {
	struct OutputNode : NodeWithCategory<NodeCategory::UNIQUE> {
		explicit OutputNode() : NodeWithCategory("Output") {
			inputPin = addCodeIN("Shader", ConnectionType::Float4, "");
		}
		std::string resolveFinal() const {
			if (!inputPin->isConnected()) return {};
			std::string v = inputPin->val();
			const auto link = inputPin->getLink().lock();
			if (!link) return v;
			return ConvertConnectionExpression(std::move(v), pinType(link->left()), ConnectionType::Float4);
		}
		bool hasShaderInputConnected() const {
			return inputPin->isConnected();
		}
	private:
		InputPin inputPin;
	};

}