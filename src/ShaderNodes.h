//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include <glm/glm.hpp>

#include "NodeBasics.h"

namespace Andesite {

    struct PBRNode : NodeWithCategory<NodeCategory::SHADER> {
        explicit PBRNode() {
            setTitle("PBR Shader");

            inputPinAlbedo = addIN<glm::vec3>("Albedo", {}, FloatOrVec3Filter(), colorPinStyle());
            inputPinMetallic = addIN<float>("Metallic", 0.f, ImFlow::ConnectionFilter::SameType(), scalarPinStyle());
            inputPinRoughness = addIN<float>("Roughness", 0.f, ImFlow::ConnectionFilter::SameType(), scalarPinStyle());

            addOUT<OutputData>("Output", shaderPinStyle())->behaviour([this] {
                return data;
            });
        }

		void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
	        const std::string outVar = "node" + std::to_string(getUID()) + "_out";
        	const std::string albedo = inputPinAlbedo->isConnected()  ? ctx.resolveUpstreamAs(inputPinAlbedo, "float3")   : "float3(1,0,0)";
        	const std::string metallic = inputPinMetallic->isConnected()  ? ctx.resolveUpstream(inputPinMetallic) : "0.0";
        	const std::string rough = inputPinRoughness->isConnected() ? ctx.resolveUpstream(inputPinRoughness) : "0.5";

        	stream << "float4 " << outVar << " = PBR(" << albedo << ", " << metallic << ", " << rough << ");\n";
        	ctx.registerOutput(getUID(), 0, outVar, "float4");
        }
    private:
    	std::shared_ptr<ImFlow::InPin<glm::vec3>> inputPinAlbedo;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinMetallic;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinRoughness;
        OutputData data;
    };
}
