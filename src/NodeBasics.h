//
// Created by Hayden Rivas on 10/24/25.
//

#pragma once

#include <ImNodeFlow.h>
#include <nfd.h>
#include <typeindex>
#include <mythril/CTX.h>

#include <vector>
#include <unordered_map>
#include <utility>

#include "Types.h"
#include "Style.h"

namespace Andesite {
	// alias this as we use it in tons of place
	using InputPin = std::shared_ptr<ImFlow::InPin<std::string>>;
	using OutputPin = std::shared_ptr<ImFlow::OutPin<std::string>>;

	enum class NodeCategory : uint8_t {
        INPUT,
        CONVERT,
        TEXTURE,
        SHADER,

        UNIQUE
    };

    // customization types
    inline std::shared_ptr<ImFlow::NodeStyle> NodeStyleFromCategory(NodeCategory category) {
        switch (category) {
            case NodeCategory::INPUT: return inputStyle();
            case NodeCategory::CONVERT: return convertStyle();
            case NodeCategory::TEXTURE: return textureStyle();
            case NodeCategory::SHADER: return shaderStyle();
            case NodeCategory::UNIQUE: return uniqueStyle();
        		default: assert(false && "Unhandled case from NodeStyleFromCategory");
        }
    }

    // do not include uniques in the order, it is always last
    static constexpr NodeCategory kCATEGORY_ORDER[] = {
        NodeCategory::INPUT,
        NodeCategory::CONVERT,
        NodeCategory::TEXTURE,
        NodeCategory::SHADER,
    };
    static constexpr size_t kCATEGORY_COUNT = sizeof(kCATEGORY_ORDER) / sizeof(NodeCategory);

    class NodeRegistry {
        struct NodeDefinition {
            std::string name;
            NodeCategory category;
            std::function<std::shared_ptr<ImFlow::BaseNode>(ImFlow::ImNodeFlow& flow)> func;
        	// as of now only used for ui purposes
            std::vector<ConnectionType> acceptedInputs;   // output types this node's inputs will accept
            std::vector<ConnectionType> providedOutputs;  // connection types this node's outputs emit
        };
    public:
        template<std::derived_from<INode> T>
        static void RegisterNode(const std::string& name, NodeCategory category) {
            NodeDefinition def = {};
            def.name = name;
            def.category = category;
            def.func = [](ImFlow::ImNodeFlow& flow) -> std::shared_ptr<ImFlow::BaseNode> {
                return flow.placeNode<T>();
            };
            // on initial registration, check all inputs and outputs of the node
            // and added them to lists of what is accepted
            {
                T probe;
                if constexpr (requires { T::AcceptedInputs(); }) {
                    def.acceptedInputs = T::AcceptedInputs();
                } else {
                    for (const auto& pin : probe.getIns()) {
                        def.acceptedInputs.push_back(pinType(pin.get()));
                    }
                }
                if constexpr (requires { T::ProvidedOutputs(); }) {
                    def.providedOutputs = T::ProvidedOutputs();
                } else {
                    for (const auto& pin : probe.getOuts()) {
                        def.providedOutputs.push_back(pinType(pin.get()));
                    }
                }
                for (const auto& pin : probe.getIns())
                    clearPinMetadata(pin.get());
                for (const auto& pin : probe.getOuts())
                    clearPinMetadata(pin.get());
            }
        	// add to category list
            categorizedNodes[category].push_back(def);
        	// add to complete list
        	// keep sorted alphabetically on the fly
        	auto it = std::lower_bound(allNodes.begin(), allNodes.end(), def.name,
			[](const NodeDefinition& node, const std::string& targetName) {
				return node.name < targetName;
			});
        	allNodes.insert(it, std::move(def));
        }
        static const std::vector<NodeDefinition>& GetNodesInCategory(NodeCategory category) {
            static std::vector<NodeDefinition> empty;
            const auto& it = categorizedNodes.find(category);
            return it != categorizedNodes.end() ? it->second : empty;
        }
    	static const std::vector<NodeDefinition>& GetAllNodes() {
        	return allNodes;
        }
    private:
        static std::unordered_map<NodeCategory, std::vector<NodeDefinition>> categorizedNodes;
    	inline static std::vector<NodeDefinition> allNodes;
    };

    constexpr const char* NodeCategoryToString(NodeCategory category) {
        switch (category) {
            case NodeCategory::INPUT: return "Input";
            case NodeCategory::CONVERT: return "Convert";
            case NodeCategory::SHADER: return "Shader";
            case NodeCategory::TEXTURE: return "Texture";

            case NodeCategory::UNIQUE: return "INVALID AS CATEGORY";
        	default: assert(false && "Unhandled case in NodeCategoryToString.");
        }
    }

	// use this when representing any node
	// do not inherit from directly
	struct INode : ImFlow::BaseNode {
    	~INode() override = default;
    	explicit INode(NodeCategory category) : _category(category) {}
    	virtual NodeCategory getCategory() const { return _category; }
    	virtual uint64_t stateHash() const { return 0; }
    protected:
    	// wrappers
    	// dont ever call the standard addOUT or addIN provided by ImNodeFlow
    	OutputPin addCodeOUT(const std::string& name, ConnectionType connectionType, PinStyle style = nullptr) {
    		auto pin = addOUT<std::string>(name, style ? std::move(style) : ConnectionTypeToStyle(connectionType));
    		setPinType(pin.get(), connectionType);
    		return pin;
    	}
    	// by default always use CodeFilterCompatible()
    	InputPin addCodeIN(const std::string& name, ConnectionType connectionType, std::string defaultExpr, PinStyle style = nullptr) {
    		return addCodeIN(name, connectionType, std::move(defaultExpr), CodeFilterCompatible(), std::move(style));
    	}
    	InputPin addCodeIN(const std::string& name, ConnectionType connectionType, std::string defaultExpr, std::function<bool(ImFlow::Pin*, ImFlow::Pin*)> filter, PinStyle style = nullptr) {
    		auto pin = addIN<std::string>(name, std::move(defaultExpr), std::move(filter), style ? std::move(style) : ConnectionTypeToStyle(connectionType));
    		setPinType(pin.get(), connectionType);
    		return pin;
    	}

    	// get a specified input pin's value
    	std::string pull(const char* name) { return getInVal<std::string>(name); }
    	// pull but casts types from one to another
    	std::string pullAs(const char* name, ConnectionType targetConnection) {
    		auto v = getInVal<std::string>(name);
    		auto* p = dynamic_cast<ImFlow::InPin<std::string>*>(inPin<std::string>(name));
    		const auto link = p->getLink().lock();
    		if (!link) return v;
    		const ConnectionType srcConnection = pinType(link->left());
    		return ConvertConnectionExpression(std::move(v), srcConnection, targetConnection);
    	}
    private:
    	NodeCategory _category;
    };


	// use this when creating a node definition
	template <NodeCategory C>
	struct NodeWithCategory : INode {
		// for registration
		static constexpr NodeCategory Category = C;
		explicit NodeWithCategory(const std::string& name) : INode(C) {
			setStyle(NodeStyleFromCategory(C));
			setTitle(name);
		}
	};

	// do not inherit directly
	struct IVariableBehavior {
		virtual ~IVariableBehavior() = default;
		virtual VariableType geVariableType() const = 0;
	};


	// only necessary for nodes that guarantee their type
	template<typename T> inline constexpr auto kPCType  = VariableType::Float;
	template<> inline constexpr auto kPCType<float>     = VariableType::Float;
	template<> inline constexpr auto kPCType<glm::vec2> = VariableType::Float2;
	template<> inline constexpr auto kPCType<glm::vec3> = VariableType::Float3;
	template<> inline constexpr auto kPCType<glm::vec4> = VariableType::Float4;
	template<> inline constexpr auto kPCType<uint64_t>  = VariableType::Texture2D;

	// use when creating node definition that allows input
	template<typename T>
	struct NodeWithStaticVariable : IVariableBehavior {
		VariableType geVariableType() const final { return kPCType<T>; }
		virtual T getValue() const = 0;
	};
	// only necessary for a node that chanegs its type/size during runtime, only for advanced uses
	struct NodeWithDynamicVariable : IVariableBehavior {
		VariableType geVariableType() const final { return _runtimePushConstantType; }
		virtual const void* getRawDataPtr() const = 0;
		virtual size_t getDataSize() const = 0;
	protected:
		VariableType _runtimePushConstantType = VariableType::Float;
	};
}
