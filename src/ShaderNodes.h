//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include <glm/glm.hpp>

#include "NodeBasics.h"

namespace Andesite {

    struct PBRNode : NodeWithCategory<NodeCategory::SHADER> {
        explicit PBRNode() : NodeWithCategory("PBR Shader") {
            inputPinAlbedo = addCodeIN("Albedo", ConnectionType::Float3, "float3(1,0,0)");
            inputPinMetallic = addCodeIN("Metallic", ConnectionType::Float, "0.0");
            inputPinRoughness = addCodeIN("Roughness", ConnectionType::Float, "0.5");

            addCodeOUT("Output", ConnectionType::Float4)->behaviour([this] {
                auto& ctx = *GeneratorContext::active;
                const std::string outVar = "node" + std::to_string(getUID()) + "_out";
                const std::string albedo = pullAs("Albedo", ConnectionType::Float3);
                const std::string metallic = pull("Metallic");
                const std::string rough = pull("Roughness");
                ctx.body << "float4 " << outVar << " = PBR(" << albedo << ", " << metallic << ", " << rough << ");\n";
                return outVar;
            });
        }
    private:
    	InputPin inputPinAlbedo;
    	InputPin inputPinMetallic;
    	InputPin inputPinRoughness;
    };
}
