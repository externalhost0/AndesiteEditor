//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include "NodeBasics.h"

namespace Andesite {

    struct ScalarNode : NodeWithCategory<NodeCategory::INPUT>, NodeWithDynamicPushConstant {
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
        explicit ScalarNode() {
            setTitle("Scalar");
			updateTypeConfiguration();
        }
    	void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
        	const std::string outVar = "val" + std::to_string(this->getUID()) + "_out";
			const char* shaderTypeStr = PushConstantTypeToShaderString(_runtimePushConstantType);
        	stream << shaderTypeStr << " " << outVar << " = push.user.val_" << std::to_string(this->getUID()) << ";\n";
        	ctx.registerOutput(getUID(), 0, outVar, shaderTypeStr);
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
				case ScalarType::Int8:   _value = int8_t(0); break;
				case ScalarType::Int16:  _value = int16_t(0); break;
				case ScalarType::Int32:  _value = int32_t(0); break;
				case ScalarType::Int64:  _value = int64_t(0); break;
				case ScalarType::UInt8:  _value = uint8_t(0); break;
				case ScalarType::UInt16: _value = uint16_t(0); break;
				case ScalarType::UInt32: _value = uint32_t(0); break;
				case ScalarType::UInt64: _value = uint64_t(0); break;
				case ScalarType::Float:  _value = 0.0f; break;
				case ScalarType::Double: _value = 0.0; break;
				case ScalarType::Bool:   _value = false; break;
				default: assert(false && "Unhandled case for ScalarType in changeVariantType");
			}
			updateTypeConfiguration();
		}
		void updateTypeConfiguration() {
			dropOUT(_lastPinUID);
			std::visit([this](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				_lastPinUID = addOUT<T>("Value", scalarPinStyle())->getUid();
			}, _value);

			switch (_currentType) {
				case ScalarType::Int8:   _runtimePushConstantType = PushConstantType::Int8; break;
				case ScalarType::Int16:  _runtimePushConstantType = PushConstantType::Int16; break;
				case ScalarType::Int32:  _runtimePushConstantType = PushConstantType::Int32; break;
				case ScalarType::Int64:  _runtimePushConstantType = PushConstantType::Int64; break;
				case ScalarType::UInt8:  _runtimePushConstantType = PushConstantType::UInt8; break;
				case ScalarType::UInt16: _runtimePushConstantType = PushConstantType::UInt16; break;
				case ScalarType::UInt32: _runtimePushConstantType = PushConstantType::UInt32; break;
				case ScalarType::UInt64: _runtimePushConstantType = PushConstantType::UInt64; break;
				case ScalarType::Float:  _runtimePushConstantType = PushConstantType::Float; break;
				case ScalarType::Double: _runtimePushConstantType = PushConstantType::Double; break;
				case ScalarType::Bool:   _runtimePushConstantType = PushConstantType::Bool; break;
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

		ImFlow::PinUID _lastPinUID;
		bool _advancedOn = false;
		ScalarType _currentType = ScalarType::Float;
		ScalarValue _value = 0.0f;
    };

    struct RGBNode : NodeWithCategory<NodeCategory::INPUT>, NodeWithPushConstant<glm::vec3> {
        explicit RGBNode() {
            setTitle("RGB");

            addOUT<glm::vec3>("Color", colorPinStyle())->behaviour([this] {
                return color;
            });
        }
    	void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
        	const std::string outVar = "val" + std::to_string(this->getUID()) + "_out";
	        stream << "float3 " << outVar << " = push.user.val_" << std::to_string(this->getUID()) << ";\n";
        	ctx.registerOutput(getUID(), 0, outVar, "float3");
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
}
