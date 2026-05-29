//
// Created by Hayden Rivas on 5/28/26.
//

#pragma once

#include "NodeBasics.h"

namespace Andesite {
	struct TextureNode : NodeWithCategory<NodeCategory::TEXTURE>, NodeWithStaticVariable<uint64_t> {
		explicit TextureNode() : NodeWithCategory("Texture") {
			_inputPinUV = addCodeIN("UV", ConnectionType::Float2, "input.UV");
			addCodeOUT("Color", ConnectionType::Float4)->behaviour([this] {
				return emit();
			});
		}

		std::string emit() {
			auto& ctx = *GeneratorContext::active;
			const std::string uid    = std::to_string(getUID());
			const std::string uvVar  = "uv_" + uid;
			const std::string outVar = "node" + uid + "_color";
			const std::string uvExpr = pullAs("UV", ConnectionType::Float2);

			ctx.body << "float2 " << uvVar << " = " << uvExpr << ";\n";

			const char* samplerField = _useLinear ? "push.linearSampler" : "push.nearestSampler";
			ctx.body << "float4 " << outVar << " = push.user.val_" << uid << ".Sample(" << samplerField << ", " << uvVar << ");\n";
			ctx.addPushConstant(this, geVariableType(), &_textureIndex);
			return outVar;
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
		InputPin _inputPinUV;
		uint64_t _textureIndex = 0;
		mythril::TextureHandle _texHandle;
		bool _useLinear        = true;
		bool _pathDirty        = false;
		std::filesystem::path _path;
	};

}