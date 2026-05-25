//
// Created by Hayden Rivas on 10/24/25.
//

#pragma once

#include <ImNodeFlow.h>
#include <nfd.h>
#include <mythril/CTX.h>

#include <vector>
#include <unordered_map>

#include "Types.h"
#include "Style.h"

namespace Andesite {
	enum class NodeCategory : uint8_t {
        INPUT,
        CONVERT,
        TEXTURE,
        SHADER,

        SINGLES
    };

    // customization types
    std::shared_ptr<ImFlow::NodeStyle> NodeStyleFromCategory(NodeCategory category) {
        switch (category) {
            case NodeCategory::INPUT: return inputStyle();
            case NodeCategory::CONVERT: return convertStyle();
            case NodeCategory::TEXTURE: return textureStyle();
            case NodeCategory::SHADER: return shaderStyle();
            case NodeCategory::SINGLES: return singleStyle();
        		default: assert(false && "Unhandled case from NodeStyleFromCategory");
        }
    }

    // do not include singles in the order, it is always last
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
            std::function<void(ImFlow::ImNodeFlow& flow)> func;
        };
    public:
        template<typename T>
        static void RegisterNode(const std::string& name, NodeCategory category) {
            NodeDefinition def = {};
            def.name = name;
            def.category = category;
            def.func = [](ImFlow::ImNodeFlow& flow) {
                flow.placeNode<T>();
            };
            categorizedNodes[category].push_back(def);
        }
        static std::vector<NodeDefinition> GetNodesInCategory(NodeCategory category) {
            static std::vector<NodeDefinition> empty;
            const auto& it = categorizedNodes.find(category);
            return it != categorizedNodes.end() ? it->second : empty;
        }

    private:
        static std::unordered_map<NodeCategory, std::vector<NodeDefinition> > categorizedNodes;
    };

    constexpr const char* NodeCategoryToString(NodeCategory category) {
        switch (category) {
            case NodeCategory::INPUT: return "Input";
            case NodeCategory::CONVERT: return "Convert";
            case NodeCategory::SHADER: return "Shader";
            case NodeCategory::TEXTURE: return "Texture";

            case NodeCategory::SINGLES: return "INVALID AS CATEGORY";
        	default: assert(false && "Unhandled case in NodeCategoryToString.");
        }
    }

	// use this when representing a node
	// do not inherit
	struct INode : ImFlow::BaseNode {
    	explicit INode(NodeCategory category) : _category(category) {}
    	virtual NodeCategory getCategory() const { return _category; }
    	virtual uint64_t stateHash() const { return 0; }
    	virtual void emitSource(std::stringstream& stream, GeneratorContext& ctx) = 0;
    private:
    	NodeCategory _category;
    };
	// use this when creating a node definition
	template <NodeCategory C>
	struct NodeWithCategory : INode {
		// for registration
		static constexpr NodeCategory Category = C;
		explicit NodeWithCategory() : INode(C) {
			setStyle(NodeStyleFromCategory(C));
		}
	};

	// do not inherit
	struct IPushConstantBehavior {
		virtual ~IPushConstantBehavior() = default;
		virtual PushConstantType getPCType() const = 0;
	};


	// only necessary for nodes that guarantee their type
	template<typename T> inline constexpr auto kPCType  = PushConstantType::Float;
	template<> inline constexpr auto kPCType<float>     = PushConstantType::Float;
	template<> inline constexpr auto kPCType<glm::vec2> = PushConstantType::Float2;
	template<> inline constexpr auto kPCType<glm::vec3> = PushConstantType::Float3;
	template<> inline constexpr auto kPCType<glm::vec4> = PushConstantType::Float4;
	template<> inline constexpr auto kPCType<uint64_t>  = PushConstantType::Texture2D;

	// use when creating node definition that allows input
	template<typename T>
	struct NodeWithPushConstant : IPushConstantBehavior {
		PushConstantType getPCType() const final { return kPCType<T>; }
		virtual T getValue() const = 0;
	};
	struct NodeWithDynamicPushConstant : IPushConstantBehavior {
		PushConstantType getPCType() const final { return _runtimePushConstantType; }
		virtual const void* getRawDataPtr() const = 0;
		virtual size_t getDataSize() const = 0;
	protected:
		PushConstantType _runtimePushConstantType = PushConstantType::Float;
	};

	// accepts float, vec3, or vec4, as it gets swizzled to .xyz in resolveUpstreamAs
	static std::function<bool(ImFlow::Pin*, ImFlow::Pin*)> FloatOrVec3Filter() {
		return [](const ImFlow::Pin* out, ImFlow::Pin* /*in*/) {
			return out->getDataType() == typeid(glm::vec3)
			    || out->getDataType() == typeid(glm::vec4)
			    || out->getDataType() == typeid(float);
		};
	}

#define REGISTER_NODE(Type, Name) \
	inline const bool Type##_registered = []() { \
		NodeRegistry::RegisterNode<Type>(Name, Type::Category); \
		return true; \
	}()


    struct OutputData { int f = 0; };

    struct OutputNode : NodeWithCategory<NodeCategory::SINGLES> {
        explicit OutputNode() {
            setTitle("Output");

            inputPin = addIN<OutputData>("Shader", {}, ImFlow::ConnectionFilter::SameType(), shaderPinStyle());
        }
    	void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
        	if (inputPin->isConnected()) {
		        const std::string inVar = ctx.resolveUpstream(inputPin);
		        stream << "output.FragColor = " << inVar << ";\n";
        	}
        }
        bool hasShaderInputConnected() const {
	        return inputPin->isConnected();
        }
    private:
    	std::shared_ptr<ImFlow::InPin<OutputData>> inputPin;
    };

	struct TextureNode : NodeWithCategory<NodeCategory::TEXTURE>, NodeWithPushConstant<uint64_t> {
		explicit TextureNode() {
			setTitle("Texture");
			inputPinUV = addIN<glm::vec2>("UV", {}, ImFlow::ConnectionFilter::SameType(), vectorPinStyle());
			addOUT<glm::vec4>("Color", colorPinStyle())->behaviour([this] { return glm::vec4(0.f); });
		}

		void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
			const std::string uid    = std::to_string(getUID());
			const std::string uvVar  = "uv_" + uid;
			const std::string outVar = "node" + uid + "_color";

			if (inputPinUV->isConnected())
				stream << "float2 " << uvVar << " = " << ctx.resolveUpstream(inputPinUV) << ";\n";
			else
				stream << "float2 " << uvVar << " = input.UV;\n";

			const char* samplerField = _useLinear ? "push.linearSampler" : "push.nearestSampler";
			stream << "float4 " << outVar << " = push.user.val_" << uid
			       << ".Sample(" << samplerField << ", " << uvVar << ");\n";
			ctx.registerOutput(getUID(), 0, outVar, "float4");
		}

		void draw() override {
			if (_texHandle.valid()) ImGui::Image(_texHandle, {100, 100});

			ImGui::SetNextItemWidth(200.f);
			if (ImGui::Button(std::format("File Dialog: {}", _path.filename().string()).c_str())) {
				nfdopendialogu8args_t args = {nullptr};
				constexpr nfdu8filteritem_t filters[] = {{ "Images", "png,jpg,jpeg,JPEG,tga"} };
				args.filterList = filters;
				args.filterCount = 1;

				// needs to be scoped here to preserve lifetime
				std::string default_path_str;
				if (!_path.empty() && std::filesystem::exists(_path)) {
					default_path_str = _path.string();
					args.defaultPath = default_path_str.c_str();
				}
				nfdu8char_t* out_path = nullptr;
				const nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);
				if (result == NFD_OKAY) {
					if (out_path != nullptr) {
						_path = out_path;
						NFD_FreePathU8(out_path);
						_pathDirty = true;
					}
				}
			}
			ImGui::SetNextItemWidth(100.f);
			if (ImGui::BeginCombo("##sampler", _useLinear ? "Linear" : "Nearest")) {
				if (ImGui::Selectable("Linear", _useLinear))  _useLinear = true;
				if (ImGui::Selectable("Nearest", !_useLinear)) _useLinear = false;
				ImGui::EndCombo();
			}
		}

		uint64_t getValue() const override { return _textureIndex; }
		void setTextureIndex(uint64_t idx) { _textureIndex = idx; }
		void applyHandle(mythril::TextureHandle handle) { _texHandle = handle; }
		const std::filesystem::path& getFilePath() const { return _path; }
		bool isPathDirty() const { return _pathDirty; }
		void clearPathDirty() { _pathDirty = false; }

		uint64_t stateHash() const override {
			uint64_t h = std::hash<std::filesystem::path>{}(_path);
			h ^= std::hash<bool>{}(_useLinear) + 0x9e3779b9u + (h << 6) + (h >> 2);
			return h;
		}

	private:
		std::shared_ptr<ImFlow::InPin<glm::vec2>> inputPinUV;
		uint64_t _textureIndex = 0;
		mythril::TextureHandle _texHandle;
		bool _useLinear        = true;
		bool _pathDirty        = false;
		std::filesystem::path _path;
	};
}
