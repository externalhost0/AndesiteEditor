//
// Created by Hayden Rivas on 5/21/26.
//

#pragma once
#include <unordered_set>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>


#include "NodeBasics.h"
#include "Types.h"
#include "UniqueNodes.h"

namespace Andesite {
	static constexpr uint32_t kGOLDEN_RATIO = 0x9e3779b9;

	struct CompileResult {
		std::string fileContent;
		std::vector<VariableEntry> bufferLayout;
		size_t bufferSize;
	};

	struct TypeLayout {
		size_t size;
		size_t alignment;
	};
	static TypeLayout TypeLayoutFromVariableType(VariableType type) {
		switch (type) {
			case VariableType::Float:    return {.size = 4, .alignment = 4};
			case VariableType::Float2:   return {.size = 8, .alignment = 4};
			case VariableType::Float3:   return {.size = 12, .alignment = 4};
			case VariableType::Float4:   return {.size = 16, .alignment = 4};

			case VariableType::Double:   return {.size = 8,  .alignment = 8};

			case VariableType::Int8:     return {.size = 1,  .alignment = 1};
			case VariableType::Int16:    return {.size = 2,  .alignment = 2};
			case VariableType::Int32:    return {.size = 4,  .alignment = 4};
			case VariableType::Int64:    return {.size = 8,  .alignment = 8};

			case VariableType::UInt8:    return {.size = 1,  .alignment = 1};
			case VariableType::UInt16:   return {.size = 2,  .alignment = 2};
			case VariableType::UInt32:   return {.size = 4,  .alignment = 4};
			case VariableType::UInt64:   return {.size = 8,  .alignment = 8};

				// https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/02-conventional-features.html#boolean-type
			case VariableType::Bool:     return {.size = 1,  .alignment = 1};

				// slang places descriptor_handles to uint2 via c layout
			case VariableType::Texture2D: return {.size = 8, .alignment = 4};
			default: assert(false && "Unhanded PushConstantType!");
		}
	}

	// required for struct packing to match with gpu
	static size_t AlignUp(size_t value, size_t alignment) {
		assert(alignment != 0 && "Alignment must be non-zero");
		return (value + alignment - 1) / alignment * alignment;
	}

	inline void GeneratorContext::addPushConstant(INode* node, VariableType type, const void* src, const std::string& slot) {
		const auto [size, alignment] = TypeLayoutFromVariableType(type);
		const size_t offset = AlignUp(pcSize, alignment);
		pushConstants.push_back({
			.varName = "val_" + std::to_string(node->getUID()) + slot,
			.type = type,
			.offset = offset,
			.size = size,
			.src = src,
		});
		pcSize = offset + size;
	}

	struct GraphSignature {
		static uint64_t Compute(OutputNode* output_node) {
			// basically perform the dfs we did before but only to test if we need to recompile
			uint64_t sig = 0;
			std::unordered_set<ImFlow::NodeUID> visited;
			hashRecursive(output_node, sig, visited);
			return sig;
		}
	private:
		static void mix(uint64_t& sig, uint64_t v) {
			sig ^= std::hash<uint64_t>{}(v) + kGOLDEN_RATIO + (sig << 6) + (sig >> 2);
		}
		static void hashRecursive(ImFlow::BaseNode* basenode, uint64_t& sig, std::unordered_set<ImFlow::NodeUID>& visited) {
			if (!basenode) return;
			const ImFlow::NodeUID uid = basenode->getUID();
			if (visited.contains(uid)) return;
			visited.insert(uid);

			mix(sig, uid);
			if (const auto* inode = dynamic_cast<INode*>(basenode)) {
				mix(sig, inode->stateHash());
			}
			for (const auto& input_pin : basenode->getIns()) {
				if (!input_pin->isConnected()) continue;
				const auto link = input_pin->getLink().lock();
				if (!link) continue;
				mix(sig, input_pin->getUid());
				mix(sig, link->left()->getUid());
				hashRecursive(link->left()->getParent(), sig, visited);
			}
		}
	};

	static std::string PushConstantEntriesToString(const std::vector<VariableEntry>& entries) {
		if (entries.empty()) {
			std::cerr << "PushConstantEntries has size of 0\n";
		}
		std::string result;
		for (const auto& entry : entries) {
			result += std::format("\n\t{} {};", VariableTypeToCodegenString(entry.type), entry.varName);
		}
		return result;
	}

	struct ShaderCompiler {
		static CompileResult Compile(OutputNode* output_node) {
			GeneratorContext ctx;
			GeneratorContext::Scope guard(&ctx);
			const std::string finalVar = output_node->resolveFinal();
			if (!finalVar.empty())
				ctx.body << "output.FragColor = " << finalVar << ";\n";

			// fixme: these paths need to be easily findable in any environment
			std::ifstream shared_top("../../assets/shaders/SharedTop.txt");
			if (!shared_top.is_open()) {
				std::cerr << "Failed to open SharedTop.txt for reading. "<< std::endl;
				assert(false);
			}
			std::ifstream mixfunctions("../../assets/shaders/MixFunctions.txt");
			if (!mixfunctions.is_open()) {
				std::cerr << "Failed to open MixFunctions.txt for reading." << std::endl;
				assert(false);
			}
			std::ifstream shared_bottom("../../assets/shaders/SharedBottom.txt");
			if (!shared_bottom.is_open()) {
				std::cerr << "Failed to open SharedBottom.txt for reading. "<< std::endl;
				assert(false);
			}

			const std::string members = PushConstantEntriesToString(ctx.pushConstants);
			const std::string userStructDef = "struct UserData {" + members + "\n}\n";
			const static std::string pc = "struct PushConstants { \n\tfloat4x4 model;\n\tPtr<Vertex> vertexBufferAddress;\n\tPtr<PerFrameData> frame;\n\tDescriptorHandle<SamplerState> linearSampler;\n\tDescriptorHandle<SamplerState> nearestSampler;\n\tPtr<UserData> user;\n}\n";

			std::stringstream final;

			final << "// Generated by Andesite // \n";
			final << shared_top.rdbuf();
			final << userStructDef;
			final << pc;
			final << shared_bottom.rdbuf();
			final << mixfunctions.rdbuf();
			final << "[shader(\"pixel\")]\n";
			final << "FSOutput fs_main(v2f input) {\n";
			final << "FSOutput output;\n";
			final << ctx.body.str();
			final << "return output;\n";
			final << "}\n";

			shared_top.close();
			mixfunctions.close();
			shared_bottom.close();

			return { final.str(), ctx.pushConstants, ctx.pcSize };
		}
	};
}
