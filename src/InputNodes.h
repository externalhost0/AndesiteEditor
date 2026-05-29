//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include "NodeBasics.h"

// generally nodes that have no inputs themselves
namespace Andesite {

    struct ScalarNode : NodeWithCategory<NodeCategory::INPUT>, NodeWithDynamicVariable {
		using ScalarValue = std::variant<
			int8_t,
			int16_t,
			int32_t,
			int64_t,
			uint8_t,
			uint16_t,
			uint32_t,
			uint64_t,
			float,
			double,
			bool
		>;
		const void* getRawDataPtr() const override {
			return std::visit([](const auto& arg) -> const void* { return &arg; }, _value);
		}
		size_t getDataSize() const override {
			return std::visit([](const auto& arg) { return sizeof(arg); }, _value);
		}
		uint64_t stateHash() const override {
			return static_cast<uint64_t>(_currentType);
		}
        explicit ScalarNode() : NodeWithCategory("Scalar") {
			updateTypeConfiguration();
			addCodeOUT("Value", ConnectionType::Float)->behaviour([this] {
				auto& ctx = *GeneratorContext::active;
				const std::string outVar = "val" + std::to_string(getUID()) + "_out";
				const char* shaderTypeStr = VariableTypeToCodegenString(_runtimePushConstantType);
				ctx.body << shaderTypeStr << " " << outVar << " = push.user.val_" << std::to_string(getUID()) << ";\n";
				ctx.addPushConstant(this, geVariableType(), getRawDataPtr());
				return outVar;
			});
        }
        void draw() override {
        	ImGui::SetNextItemWidth(150.f);
			if (isBoolean()) {
				ImGui::Checkbox("###Value", &std::get<bool>(_value));
			} else {
				const ImGuiDataType imguiType = getImGuiDataType(_currentType);
				const float speed = isFloat() ? 0.05f : 1.0f;
				std::visit([&](auto&& arg) {
					ImGui::DragScalar("###Value", imguiType, &arg, speed);
				}, _value);
			}

        	ImGui::SetNextItemWidth(100.f);
			if (_advancedOn) {
				if (ImGui::BeginCombo("Types", ScalarTypeToReadableString(_currentType))) {
					for (int i = 0; i < static_cast<int>(ScalarType::kNUM); ++i) {
						const auto type = static_cast<ScalarType>(i);
#ifdef __APPLE__
						// double types are not supported in buffers in MSL
						if (type == ScalarType::Double) continue;
#endif
						const bool isSelected = _currentType == type;
						if (ImGui::Selectable(ScalarTypeToReadableString(type), isSelected)) {
							_currentType = type;
							changeVariantStoredValue(type);
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			ImGui::Checkbox("Advanced Mode", &_advancedOn);
        }
    private:
		bool isFloat() const {
			switch (_currentType) {
				case ScalarType::Float:
				case ScalarType::Double:
					return true;
				default: return false;
			}
		}
		bool isIntegar() const {
			switch (_currentType) {
				case ScalarType::Int8:
				case ScalarType::Int16:
				case ScalarType::Int32:
				case ScalarType::Int64:
				case ScalarType::UInt8:
				case ScalarType::UInt16:
				case ScalarType::UInt32:
				case ScalarType::UInt64:
					return true;
				default: return false;
			}
		}
		bool isBoolean() const {
			return _currentType == ScalarType::Bool;
		}

		void changeVariantStoredValue(ScalarType type) {
			switch (type) {
				case ScalarType::Int8:   _value = static_cast<int8_t>(0); break;
				case ScalarType::Int16:  _value = static_cast<int16_t>(0); break;
				case ScalarType::Int32:  _value = static_cast<int32_t>(0); break;
				case ScalarType::Int64:  _value = static_cast<int64_t>(0); break;
				case ScalarType::UInt8:  _value = static_cast<uint8_t>(0); break;
				case ScalarType::UInt16: _value = static_cast<uint16_t>(0); break;
				case ScalarType::UInt32: _value = static_cast<uint32_t>(0); break;
				case ScalarType::UInt64: _value = static_cast<uint64_t>(0); break;
				case ScalarType::Float:  _value = 0.0f; break;
				case ScalarType::Double: _value = 0.0; break;
				case ScalarType::Bool:   _value = false; break;
				default: assert(false && "Unhandled case for ScalarType in changeVariantType");
			}
			updateTypeConfiguration();
		}
		void updateTypeConfiguration() {
			switch (_currentType) {
				case ScalarType::Int8:   _runtimePushConstantType = VariableType::Int8; break;
				case ScalarType::Int16:  _runtimePushConstantType = VariableType::Int16; break;
				case ScalarType::Int32:  _runtimePushConstantType = VariableType::Int32; break;
				case ScalarType::Int64:  _runtimePushConstantType = VariableType::Int64; break;
				case ScalarType::UInt8:  _runtimePushConstantType = VariableType::UInt8; break;
				case ScalarType::UInt16: _runtimePushConstantType = VariableType::UInt16; break;
				case ScalarType::UInt32: _runtimePushConstantType = VariableType::UInt32; break;
				case ScalarType::UInt64: _runtimePushConstantType = VariableType::UInt64; break;
				case ScalarType::Float:  _runtimePushConstantType = VariableType::Float; break;
				case ScalarType::Double: _runtimePushConstantType = VariableType::Double; break;
				case ScalarType::Bool:   _runtimePushConstantType = VariableType::Bool; break;
					default: assert(false && "Unhanled ScalarType");
			}
		}
		static ImGuiDataType getImGuiDataType(ScalarType type) {
			switch (type) {
				case ScalarType::Int8:   return ImGuiDataType_S8;
				case ScalarType::Int16:  return ImGuiDataType_S16;
				case ScalarType::Int32:  return ImGuiDataType_S32;
				case ScalarType::Int64:  return ImGuiDataType_S64;
				case ScalarType::UInt8:  return ImGuiDataType_U8;
				case ScalarType::UInt16: return ImGuiDataType_U16;
				case ScalarType::UInt32: return ImGuiDataType_U32;
				case ScalarType::UInt64: return ImGuiDataType_U64;
				case ScalarType::Double: return ImGuiDataType_Double;
				// just use float
				default: return ImGuiDataType_Float;
			}
		}

		bool _advancedOn = false;
		ScalarType _currentType = ScalarType::Float;
		ScalarValue _value = 0.0f;
    };

    struct RGBNode : NodeWithCategory<NodeCategory::INPUT>, NodeWithStaticVariable<glm::vec3> {
        explicit RGBNode() : NodeWithCategory("RGB") {
            addCodeOUT("Color", ConnectionType::Float3)->behaviour([this] {
                auto& ctx = *GeneratorContext::active;
                const std::string outVar = "val" + std::to_string(getUID()) + "_out";
                ctx.body << "float3 " << outVar << " = push.user.val_" << std::to_string(getUID()) << ";\n";
                ctx.addPushConstant(this, geVariableType(), &color);
                return outVar;
            });
        }

        void draw() override {
            ImGui::SetNextItemWidth(200.f);
            constexpr ImGuiColorEditFlags flags =
                    ImGuiColorEditFlags_DisplayRGB |
                    ImGuiColorEditFlags_Float |
                    ImGuiColorEditFlags_NoSmallPreview |
                    ImGuiColorEditFlags_NoSidePreview |
                    ImGuiColorEditFlags_PickerHueBar;
            ImGui::ColorPicker3("##ColorPicker", reinterpret_cast<float*>(&color), flags);
        }
    	glm::vec3 getValue() const override { return color; }

    private:
        glm::vec3 color = {1, 0, 0};
    };

	struct TextureCoordinateNode : NodeWithCategory<NodeCategory::INPUT> {
		explicit TextureCoordinateNode() : NodeWithCategory("Texture Coordinate") {
			addCodeOUT("Generated", ConnectionType::Float2)->behaviour([this] {
				auto& ctx = *GeneratorContext::active;
				const std::string outVar = "val" + std::to_string(getUID()) + "_out";
				ctx.body << "float2 " << outVar << " = input.UV;\n";
				return outVar;
			});
		}
	};
}
