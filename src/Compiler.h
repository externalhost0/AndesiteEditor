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

namespace Andesite {
	static constexpr uint32_t kGOLDEN_RATIO = 0x9e3779b9;

	struct PushConstantEntry {
		std::string varName;
		PushConstantType type;
		INode* pNode;
		size_t offset;
		size_t size;
	};
	struct CompileResult {
		std::string fileContent;
		std::vector<PushConstantEntry> pushLayout;
		size_t pcSize;
	};

	struct GraphNode {
		INode* node = nullptr;
		std::vector<INode*> nodeInputs;
		bool active = false;

		bool isDirty() const { return _dirty; }
		void markDirty() { _dirty = true; }
		void clearDirty() { _dirty = false; }
	private:
		bool _dirty = false;
	};

	struct PCResult {
		std::vector<PushConstantEntry> entries;
		size_t totalsize;
	};

	struct PCTypeLayout {
		size_t size;
		size_t alignment;
	};

	static PCTypeLayout LayoutFromPCType(PushConstantType type) {
		switch (type) {
			case PushConstantType::Float: return {.size = 4, .alignment = 4};
			case PushConstantType::Float2: return {.size = 8, .alignment = 4};
			case PushConstantType::Float3: return {.size = 12, .alignment = 4};
			case PushConstantType::Float4: return {.size = 16, .alignment = 4};

			case PushConstantType::Double:   return {.size = 8,  .alignment = 8};

			case PushConstantType::Int8:     return {.size = 1,  .alignment = 1};
			case PushConstantType::Int16:    return {.size = 2,  .alignment = 2};
			case PushConstantType::Int32:    return {.size = 4,  .alignment = 4};
			case PushConstantType::Int64:    return {.size = 8,  .alignment = 8};

			case PushConstantType::UInt8:    return {.size = 1,  .alignment = 1};
			case PushConstantType::UInt16:   return {.size = 2,  .alignment = 2};
			case PushConstantType::UInt32:   return {.size = 4,  .alignment = 4};
			case PushConstantType::UInt64:   return {.size = 8,  .alignment = 8};

			// https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/02-conventional-features.html#boolean-type
			case PushConstantType::Bool:     return {.size = 4,  .alignment = 4};

			case PushConstantType::Texture2D: return {.size = 8, .alignment = 8};
			default: assert(false && "Unhanded PushConstantType!");
		}
	}
	static size_t AlignUp(size_t value, size_t alignment) {
		assert(alignment != 0 && "Alignment must be non-zero");
		return (value + alignment - 1) / alignment * alignment;
	}
	static PCResult EvaluateInputs(const std::vector<GraphNode>& topological_nodes) {
		std::vector<PushConstantEntry> result = {};
		size_t totalsize = 0;
		// just move through each node, if it inherits pc behavior than we are going to add it to result
		for (const auto& graph_node : topological_nodes) {
			INode* node = graph_node.node;
			const auto* pc_behavior = dynamic_cast<IPushConstantBehavior*>(node);
			if (!pc_behavior) continue;
			const PushConstantType pc_type = pc_behavior->getPCType();
			const PCTypeLayout layout = LayoutFromPCType(pc_type);
			const size_t offset = AlignUp(totalsize, layout.alignment);
			result.push_back({
				.varName = "val_" + std::to_string(node->getUID()),
				.type = pc_type,
				.pNode = node,
				.offset = offset,
				.size = layout.size,
			});
			totalsize = offset + layout.size;
		}
		return { result, totalsize };
	}

	struct Graph {
		static std::vector<GraphNode> Build(OutputNode* output_node) {
			std::vector<GraphNode> toposort_nodes;
			std::unordered_set<ImFlow::NodeUID> visited;
			buildRecursive(output_node, toposort_nodes, visited);
			return toposort_nodes;
		}
	private:
		size_t _nodeCount = 0;

		static INode* convertToINode(ImFlow::BaseNode* base_node) {
			const auto node = dynamic_cast<INode*>(base_node);
			assert(node && "covertToINode() failed: ImFlow::BaseNode* failed to cast into INode*");
			return node;
		}
		static void buildRecursive(ImFlow::BaseNode* basenode, std::vector<GraphNode>& post_order, std::unordered_set<ImFlow::NodeUID>& visited) {
			if (!basenode) return;
			const ImFlow::NodeUID nodeUid = basenode->getUID();
			// base case
			if (visited.contains(nodeUid)) return;

			visited.insert(nodeUid);
			GraphNode new_node;
			new_node.node = dynamic_cast<INode*>(basenode);
			for (const auto& input_pin : basenode->getIns()) {
				// obviously we should only care about inputs that have incoming data
				if (input_pin->isConnected()) {
					// a link represents the connection between two nodes
					const auto link = input_pin->getLink().lock();
					if (!link) continue;
					// getting the left pin is also the pin from the "left" or the node behind
					ImFlow::BaseNode* upstream_node = link->left()->getParent();
					// add this discovered node to the new_nodes inputs
					new_node.nodeInputs.push_back(dynamic_cast<INode*>(upstream_node));
					// as we do a dfs, continue from this discovered node
					buildRecursive(upstream_node, post_order, visited);
				}
			}
			// once the end of a node as been reached, we add it to
			post_order.push_back(new_node);
		}
	};

	struct GraphSignature {
		static uint64_t Compute(OutputNode* output_node) {
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

	static std::string PushConstantEntriesToString(const std::vector<PushConstantEntry>& entries) {
		if (entries.empty()) {
			std::cerr << "PushConstantEntries has size of 0\n";
		}
		std::string result;
		for (const auto& entry : entries) {
			result += std::format("{} {};", PushConstantTypeToShaderString(entry.type), entry.varName);
		}
		return result;
	}

	struct ShaderCompiler {
		static CompileResult Compile(OutputNode* output_node) {
			// this is also already culled
			std::vector<GraphNode> topological_nodes = Graph::Build(output_node);

			const auto [entries, totalsize] = EvaluateInputs(topological_nodes);
			GeneratorContext generator_context;
			std::stringstream body;
			for (const auto& graph_node : topological_nodes) {
				graph_node.node->emitSource(body, generator_context);
			}

			// fixme: these paths need to be easily findable in any environment
			std::ifstream shared_top("../../assets/shaders/SharedTop.txt");
			if (!shared_top.is_open()) {
				std::cerr << "Failed to open SharedTop.txt for reading. "<< std::endl;
				assert(false);
			}
			std::ifstream shared_bottom("../../assets/shaders/SharedBottom.txt");
			if (!shared_bottom.is_open()) {
				std::cerr << "Failed to open SharedBottom.txt for reading. "<< std::endl;
				assert(false);
			}

			const std::string members = PushConstantEntriesToString(entries);
			const std::string userStructDef = "struct UserData {" + members + "}\n";
			const static std::string pc = "struct PushConstants { float4x4 model; Ptr<Vertex> vertexBufferAddress; Ptr<PerFrameData> frame; DescriptorHandle<SamplerState> linearSampler; DescriptorHandle<SamplerState> nearestSampler; Ptr<UserData> user; }\n";

			std::stringstream final;

			final << "// Generated by Andesite // \n";
			final << shared_top.rdbuf();
			final << userStructDef;
			final << pc;
			final << shared_bottom.rdbuf();
			final << "[shader(\"pixel\")]\n";
			final << "FSOutput fs_main(v2f input) {\n";
			final << "FSOutput output;\n";
			final << body.str();
			final << "return output;\n";
			final << "}\n";

			shared_top.close();
			shared_bottom.close();

			return { final.str(), entries, totalsize };
		}
	};
}
