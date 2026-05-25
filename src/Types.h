//
// Created by Hayden Rivas on 5/23/26.
//

#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <memory>

#include <ImNodeFlow.h>

namespace Andesite {
	enum class ScalarType {
		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float,
		Double,
		Bool,
		kNUM
	};

	static const char* ScalarTypeToReadableString(ScalarType type) {
		switch (type) {
			case ScalarType::Int8:   return "Int8";
			case ScalarType::Int16:  return "Int16";
			case ScalarType::Int32:  return "Int32";
			case ScalarType::Int64:  return "Int64";

			case ScalarType::UInt8:  return "UInt8";
			case ScalarType::UInt16: return "UInt16";
			case ScalarType::UInt32: return "UInt32";
			case ScalarType::UInt64: return "UInt64";

			case ScalarType::Float:  return "Float";
			case ScalarType::Double: return "Double";

			case ScalarType::Bool:   return "Bool";
			default: assert(false && "Unhandled case in ScalarTypeToString");
		}
	}

	enum class PushConstantType {
		Double,
		Bool,

		Float,
		Float2,
		Float3,
		Float4,

		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,

		// ie any texture or sampler, anything by handle
		Texture2D
	};
	static const char* PushConstantTypeToShaderString(PushConstantType type) {
		switch (type) {
			case PushConstantType::Float: return "float";
			case PushConstantType::Float2: return "float2";
			case PushConstantType::Float3: return "float3";
			case PushConstantType::Float4: return "float4";
			case PushConstantType::Int8:   return "int8_t";
			case PushConstantType::Int16:  return "int16_t";
			case PushConstantType::Int32:  return "int";
			case PushConstantType::Int64:  return "int64_t";
			case PushConstantType::UInt8:  return "uint8_t";
			case PushConstantType::UInt16: return "uint16_t";
			case PushConstantType::UInt32: return "uint";
			case PushConstantType::UInt64: return "uint64_t";
			case PushConstantType::Double: return "double";
			case PushConstantType::Bool:   return "bool";
			case PushConstantType::Texture2D: return "DescriptorHandle<Texture2D>";
			default: assert(false && "Unhanded case in StringFromPCType");
		}
	}


	struct GeneratorContext {
		std::map<std::pair<ImFlow::NodeUID, int>, std::string> outputVarnames;
		std::map<std::pair<ImFlow::NodeUID, int>, std::string> outputTypes;

		void registerOutput(ImFlow::NodeUID uid, int pinIdx, const std::string& varName, const std::string& type = "") {
			outputVarnames[{uid, pinIdx}] = varName;
			if (!type.empty()) outputTypes[{uid, pinIdx}] = type;
		}
		std::string resolveOutput(ImFlow::NodeUID uid, int pinIdx) const {
			const auto it = outputVarnames.find({uid, pinIdx});
			assert(it != outputVarnames.end() && "Output Var not found, your topological order is broken!");
			return it->second;
		}
		// gets whatever node is feeding into the inputPin
		template<typename T>
		std::string resolveUpstream(std::shared_ptr<ImFlow::InPin<T>> pin) const {
			auto link = pin->getLink().lock();
			assert(link && "resolveUpstream called on unconnected pin");
			ImFlow::Pin* leftPin = link->left();
			ImFlow::BaseNode* upstreamNode = leftPin->getParent();
			const auto& outs = upstreamNode->getOuts();
			auto it = std::find_if(outs.begin(), outs.end(),
				[&](const std::shared_ptr<ImFlow::Pin>& p) { return p.get() == leftPin; });
			int idx = (it != outs.end()) ? static_cast<int>(std::distance(outs.begin(), it)) : 0;
			return resolveOutput(upstreamNode->getUID(), idx);
		}
		// like resolveUpstream but promotes float→floatN by wrapping in targetType(x)
		// float3(x) broadcasts a scalar to all components
		template<typename T>
		std::string resolveUpstreamAs(std::shared_ptr<ImFlow::InPin<T>> pin, const std::string& targetType) const {
			auto link = pin->getLink().lock();
			assert(link && "resolveUpstreamAs called on unconnected pin");
			ImFlow::Pin* leftPin = link->left();
			ImFlow::BaseNode* upstreamNode = leftPin->getParent();
			const auto& outs = upstreamNode->getOuts();
			auto it = std::find_if(outs.begin(), outs.end(),
				[&](const std::shared_ptr<ImFlow::Pin>& p) { return p.get() == leftPin; });
			int idx = (it != outs.end()) ? static_cast<int>(std::distance(outs.begin(), it)) : 0;
			const std::string varName = resolveOutput(upstreamNode->getUID(), idx);
			const auto typeIt = outputTypes.find({upstreamNode->getUID(), idx});
			if (typeIt != outputTypes.end()) {
				const std::string& srcType = typeIt->second;
				if (srcType == "float" && targetType != "float")
					return targetType + "(" + varName + ")";
				if (srcType == "float4" && targetType == "float3")
					return varName + ".xyz";
			}
			return varName;
		}
	};

}