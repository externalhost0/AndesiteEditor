//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include <array>

#include <glm/glm.hpp>
#include "NodeBasics.h"

namespace Andesite {
	struct SeperateComponentsNode : NodeWithCategory<NodeCategory::CONVERT> {
		explicit SeperateComponentsNode() : NodeWithCategory("Seperate") {
			inputPin = addCodeIN("Input", ConnectionType::Adaptive, "float4(0,0,0,0)");
			syncComponentOutputs(ConnectionComponentCount(ConnectionType::Adaptive));
		}

		static std::vector<ConnectionType> AcceptedInputs() {
			return {ConnectionType::Adaptive};
		}

		static std::vector<ConnectionType> ProvidedOutputs() {
			return {ConnectionType::Float};
		}

		void draw() override {
			refreshAdaptiveInput();
			syncComponentOutputs(ConnectionComponentCount(effectiveInputType()));
		}

		std::string emitComp(const char* tag, char component) {
			auto& ctx = *GeneratorContext::active;
			const ConnectionType type = effectiveInputType();
			const std::string in = pull("Input");
			const std::string out = "node" + std::to_string(getUID()) + "_out" + tag;
			if (type == ConnectionType::Float)
				ctx.body << "float " << out << " = " << in << ";\n";
			else
				ctx.body << "float " << out << " = " << in << "." << component << ";\n";
			return out;
		}

	private:
		static constexpr std::array<const char*, 4> kComponentNames = {"X", "Y", "Z", "W"};
		static constexpr std::array<char, 4> kComponentSwizzles = {'x', 'y', 'z', 'w'};

		ConnectionType effectiveInputType() const {
			if (!inputPin || !inputPin->isConnected()) return ConnectionType::Float4;
			const auto link = inputPin->getLink().lock();
			if (!link) return ConnectionType::Float4;
			const ConnectionType type = pinType(link->left());
			return IsConcreteConnection(type) ? type : ConnectionType::Float4;
		}

		void refreshAdaptiveInput() const {
			if (inputPin->isConnected()) {
				const ConnectionType type = effectiveInputType();
				setPinType(inputPin.get(), type);
				inputPin->getStyle()->color = ConnectionTypeColor(type);
				return;
			}

			setPinType(inputPin.get(), ConnectionType::Adaptive);
			inputPin->getStyle()->color = AdaptivePinColor();
		}

		void syncComponentOutputs(uint8_t count) {
			for (uint8_t i = 0; i < outputPins.size(); ++i) {
				if (i < count) {
					if (!outputPins[i]) {
						outputPins[i] = addCodeOUT(kComponentNames[i], ConnectionType::Float);
						outputPins[i]->behaviour([this, i] {
							return emitComp(kComponentNames[i], kComponentSwizzles[i]);
						});
					}
					continue;
				}

				if (outputPins[i]) {
					clearPinMetadata(outputPins[i].get());
					dropOUT(kComponentNames[i]);
					outputPins[i].reset();
				}
			}
		}
		InputPin inputPin;
		std::array<OutputPin, 4> outputPins{};
	};

	struct CombineComponentsNode : NodeWithCategory<NodeCategory::CONVERT> {
		explicit CombineComponentsNode() : NodeWithCategory("Combine XYZ") {
			outputPin = addCodeOUT("Output", ConnectionType::Adaptive);
			outputPin->behaviour([this] {
				return emit();
			});
			syncComponentInputs(ConnectionComponentCount(ConnectionType::Adaptive));
		}

		static std::vector<ConnectionType> AcceptedInputs() {
			return {ConnectionType::Float};
		}

		static std::vector<ConnectionType> ProvidedOutputs() {
			return {ConnectionType::Adaptive};
		}

		void draw() override {
			refreshAdaptiveOutput();
			syncComponentInputs(ConnectionComponentCount(effectiveOutputType()));
		}

	private:
		static constexpr std::array<const char*, 4> kComponentNames = {"X", "Y", "Z", "W"};

		std::string emit() {
			auto& ctx = *GeneratorContext::active;
			const ConnectionType type = effectiveOutputType();
			const std::string outVar = "node" + std::to_string(getUID()) + "_out";

			if (type == ConnectionType::Float) {
				const std::string x = pull("X");
				ctx.body << "float " << outVar << " = " << x << ";\n";
				return outVar;
			}

			const uint8_t count = ConnectionComponentCount(type);
			std::array<std::string, 4> components{};
			for (uint8_t i = 0; i < count; ++i) {
				components[i] = pull(kComponentNames[i]);
			}

			ctx.body << ConnectionTypeToString(type) << " " << outVar << " = " << ConnectionTypeToString(type) << "(";
			for (uint8_t i = 0; i < count; ++i) {
				if (i > 0) ctx.body << ", ";
				ctx.body << components[i];
			}
			ctx.body << ");\n";
			return outVar;
		}

		ConnectionType effectiveOutputType() {
			if (!outputPin || !outputPin->isConnected()) return ConnectionType::Float4;

			ImFlow::ImNodeFlow* flow = getHandler();
			if (!flow) return ConnectionType::Float4;

			ConnectionType bestType = ConnectionType::Adaptive;
			for (const auto& [_, node] : flow->getNodes()) {
				if (!node) continue;
				for (const auto& input : node->getIns()) {
					if (!input || !input->isConnected()) continue;
					const auto link = input->getLink().lock();
					if (!link || link->left() != outputPin.get()) continue;

					const ConnectionType type = pinType(input.get());
					if (!IsConcreteConnection(type)) continue;
					if (type == ConnectionType::Float) return ConnectionType::Float;
					if (!IsConcreteConnection(bestType) || ConnectionComponentCount(type) > ConnectionComponentCount(bestType))
						bestType = type;
				}
			}

			return IsConcreteConnection(bestType) ? bestType : ConnectionType::Float4;
		}

		void refreshAdaptiveOutput() {
			if (outputPin->isConnected()) {
				const ConnectionType type = effectiveOutputType();
				setPinType(outputPin.get(), type);
				outputPin->getStyle()->color = ConnectionTypeColor(type);
				return;
			}

			setPinType(outputPin.get(), ConnectionType::Adaptive);
			outputPin->getStyle()->color = AdaptivePinColor();
		}

		void syncComponentInputs(uint8_t count) {
			for (uint8_t i = 0; i < inputPins.size(); ++i) {
				if (i < count) {
					if (!inputPins[i]) {
						inputPins[i] = addCodeIN(kComponentNames[i], ConnectionType::Float, "0.0");
					}
					continue;
				}

				if (inputPins[i]) {
					clearPinMetadata(inputPins[i].get());
					dropIN(kComponentNames[i]);
					inputPins[i].reset();
				}
			}
		}

		OutputPin outputPin;
		std::array<InputPin, 4> inputPins{};
	};

	enum class Operators : uint8_t {
		ADD,
		SUBTRACT,
		MULTIPLY,
		DIVIDE,

		kNUM
	};
	static const char* OperatorToString(Operators op) {
		switch (op) {
			case Operators::ADD: return "Add";
			case Operators::SUBTRACT: return "Subtract";
			case Operators::MULTIPLY: return "Multiply";
			case Operators::DIVIDE: return "Divide";
			default: assert(false);
		}
	}

    struct MathNode : NodeWithCategory<NodeCategory::CONVERT> {
        explicit MathNode() : NodeWithCategory("Math") {
            inputPinA = addCodeIN("A", ConnectionType::Float, "0.0");
            inputPinB = addCodeIN("B", ConnectionType::Float, "0.0");
            addCodeOUT("Out", ConnectionType::Float)->behaviour([this] {
                auto& ctx = *GeneratorContext::active;
                const std::string outVar = "node" + std::to_string(getUID()) + "_out";

            	const std::string a = [&] {
            		if (inputPinA->isConnected()) {
            			return pull("A");
            		}
            		ctx.addPushConstant(this, VariableType::Float, &valA, "_inputA");
            		return "push.user.val_" + std::to_string(getUID()) + "_inputA";
            	}();
            	const std::string b = [&] {
            		if (inputPinA->isConnected()) {
            			return pull("B");
            		}
            		ctx.addPushConstant(this, VariableType::Float, &valB, "_inputB");
            		return "push.user.val_" + std::to_string(getUID()) + "_inputB";
            	}();
                ctx.body << "float " << outVar << " = " << a << " " << opSignToString() << " " << b << ";\n";
                return outVar;
            });
        }
        void draw() override {
        	if (!inputPinA->isConnected()) {
        		ImGui::SetNextItemWidth(100.f);
        		ImGui::DragFloat("##valA", &valA);
        	}
        	if (!inputPinB->isConnected()) {
        		ImGui::SetNextItemWidth(100.f);
        		ImGui::DragFloat("##valB", &valB);
        	}

            ImGui::SetNextItemWidth(100.f);
            if (ImGui::BeginCombo("###Options", OperatorToString(_currentOp))) {
                for (int i = 0; i < static_cast<int>(Operators::kNUM); i++) {
                    const auto op = static_cast<Operators>(i);
                    const bool isSelected = (static_cast<int>(_currentOp) == i);
                    if (ImGui::Selectable(OperatorToString(op), isSelected)) {
                        _currentOp = op;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
		Operators getOp() const {
	        return _currentOp;
        }
    	uint64_t stateHash() const override {
	        return static_cast<uint64_t>(_currentOp);
        }
    private:
    	std::string opSignToString() const {
    		switch (_currentOp) {
    			case Operators::ADD: return "+";
    			case Operators::SUBTRACT: return "-";
    			case Operators::MULTIPLY: return "*";
    			case Operators::DIVIDE: return "/";
    			default: assert(false && "Unhandled Operators to std::string");
    		}
    	}
		float valA = 0.f;
		float valB = 0.f;
        Operators _currentOp = Operators::MULTIPLY;
    	InputPin inputPinA;
    	InputPin inputPinB;
    };

	enum class VectorOps : uint8_t {
		ADD,
		SUBTRACT,
		MULTIPLY,
		CROSS,
		NORMALIZE,
		REFLECT,
		DOT,
		LENGTH,
		SCALE,

		kNUM
	};
	static const char* VectorOpToString(VectorOps op) {
		switch (op) {
			case VectorOps::ADD:       return "Add";
			case VectorOps::SUBTRACT:  return "Subtract";
			case VectorOps::MULTIPLY:  return "Multiply";
			case VectorOps::CROSS:     return "Cross Product";
			case VectorOps::NORMALIZE: return "Normalize";
			case VectorOps::REFLECT:   return "Reflect";
			case VectorOps::DOT:       return "Dot Product";
			case VectorOps::LENGTH:    return "Length";
			case VectorOps::SCALE:     return "Scale";
			default: assert(false && "Unhandled case in VectorOpToString");
		}
	}

	// ops that only use A, pin B disappears
	static bool vectorOpIsSingleInput(VectorOps op) {
		return op == VectorOps::NORMALIZE || op == VectorOps::LENGTH;
	}

	struct VectorMathNode : NodeWithCategory<NodeCategory::CONVERT> {
		explicit VectorMathNode() : NodeWithCategory("Vector Math") {
			inputPinA = addCodeIN("A", ConnectionType::Adaptive, "float3(0,0,0)", [this](ImFlow::Pin* out, ImFlow::Pin*) {
				return canConnectVectorInput(out, true);
			});
			createInputB();
			outputPin = addCodeOUT("Out", ConnectionType::Adaptive);
			outputPin->behaviour([this] {
				return emit();
			});
			const std::vector vectorTypes = {ConnectionType::Float2, ConnectionType::Float3, ConnectionType::Float4};
			setPinConcreteOptions(inputPinA.get(), vectorTypes);
			setPinConcreteOptions(inputPinB.get(), vectorTypes);
			setPinConcreteOptions(outputPin.get(), vectorTypes);
		}

		static std::vector<ConnectionType> AcceptedInputs() {
			return {ConnectionType::Float2, ConnectionType::Float3, ConnectionType::Float4};
		}

		static std::vector<ConnectionType> ProvidedOutputs() {
			return {ConnectionType::Float2, ConnectionType::Float3, ConnectionType::Float4};
		}

		std::string emit() {
			auto& ctx = *GeneratorContext::active;
			syncInputBPin();
			refreshAdaptivePins();
			const ConnectionType type = effectiveVectorType();
			const std::string typeName = ConnectionTypeToString(type);
			const std::string vecOut = "node" + std::to_string(getUID()) + "_vec";

			const std::string a = [&] {
				if (inputPinA->isConnected()) {
					return pullAs("A", type);
				}
				ctx.addPushConstant(this, ConnectionTypeToVariableTypeEXT(type), &vecA, "_inputA");
				return "push.user.val_" + std::to_string(getUID()) + "_inputA";
			}();

			const std::string b = [&] -> std::string {
				// early return no work needed
				if (vectorOpIsSingleInput(_currentOp))
					return "";
				const bool isScale = _currentOp == VectorOps::SCALE;
				const ConnectionType used_connection_type = isScale ? ConnectionType::Float : type;
				if (inputPinB->isConnected()) {
					return pullAs("B", used_connection_type);
				}
				ctx.addPushConstant(this, ConnectionTypeToVariableTypeEXT(used_connection_type), &vecB, "_inputB");
				return "push.user.val_" + std::to_string(getUID()) + "_inputB";
			}();

			// scalar ops broadcast into the active vector type so the node keeps one vector output.
			if (_currentOp == VectorOps::DOT) {
				ctx.body << typeName << " " << vecOut << " = " << typeName << "(dot(" << a << ", " << b << "));\n";
			} else if (_currentOp == VectorOps::LENGTH) {
				ctx.body << typeName << " " << vecOut << " = " << typeName << "(length(" << a << "));\n";
			} else {
				switch (_currentOp) {
					case VectorOps::ADD:       ctx.body << typeName << " " << vecOut << " = " << a << " + " << b << ";\n"; break;
					case VectorOps::SUBTRACT:  ctx.body << typeName << " " << vecOut << " = " << a << " - " << b << ";\n"; break;
					case VectorOps::MULTIPLY:
					case VectorOps::SCALE:
						ctx.body << typeName << " " << vecOut << " = " << a << " * " << b << ";\n"; break;
					case VectorOps::CROSS:     ctx.body << typeName << " " << vecOut << " = cross(" << a << ", " << b << ");\n"; break;
					case VectorOps::NORMALIZE: ctx.body << typeName << " " << vecOut << " = normalize(" << a << ");\n"; break;
					case VectorOps::REFLECT:   ctx.body << typeName << " " << vecOut << " = reflect(" << a << ", " << b << ");\n"; break;
					default: assert(false && "Unhandled case for VectorOps in emit() for VectorMathNode!");
				}
			}
			return vecOut;
		}

		void draw() override {
			syncInputBPin();
			refreshAdaptivePins();
			if (!inputPinA->isConnected()) {
				ImGui::SetNextItemWidth(150.f);
				drawVectorControl("###vecA", vecA, effectiveVectorType());
			}
			if (inputPinB && !inputPinB->isConnected()) {
				ImGui::SetNextItemWidth(150.f);
				if (_currentOp == VectorOps::SCALE) {
					ImGui::DragFloat("###valB", &vecB.x);
				} else {
					drawVectorControl("###vecB", vecB, effectiveVectorType());
				}
			}
			ImGui::SetNextItemWidth(100.f);
			if (ImGui::BeginCombo("##VecOp", VectorOpToString(_currentOp))) {
				for (int i = 0; i < static_cast<int>(VectorOps::kNUM); i++) {
					const auto op = static_cast<VectorOps>(i);
					const bool selected = _currentOp == op;
					if (!canUseOpWithCurrentLinks(op)) {
						ImGui::BeginDisabled();
						ImGui::Selectable(VectorOpToString(op), false);
						ImGui::EndDisabled();
						continue;
					}
					if (ImGui::Selectable(VectorOpToString(op), selected))
						_currentOp = op;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			syncInputBPin();
			refreshAdaptivePins();
		}

		uint64_t stateHash() const override {
			return static_cast<uint64_t>(_currentOp);
		}

	private:
		static void drawVectorControl(const char* label, glm::vec4& value, ConnectionType type) {
			switch (type) {
				case ConnectionType::Float2: ImGui::DragFloat2(label, &value.x); break;
				case ConnectionType::Float3: ImGui::DragFloat3(label, &value.x); break;
				case ConnectionType::Float4: ImGui::DragFloat4(label, &value.x); break;
				default: assert(false && "VectorMathNode controls only support vector types");
			}
		}

		static bool vectorTypesMatch(ConnectionType a, ConnectionType b) {
			return IsVectorConnection(a) && a == b;
		}

		void createInputB() {
			if (inputPinB) return;
			inputPinB = addCodeIN("B", ConnectionType::Adaptive, "float3(0,0,0)", [this](ImFlow::Pin* out, ImFlow::Pin*) {
				return canConnectVectorInput(out, false);
			});
			setPinConcreteOptions(inputPinB.get(), {ConnectionType::Float2, ConnectionType::Float3, ConnectionType::Float4});
		}

		void syncInputBPin() {
			if (vectorOpIsSingleInput(_currentOp)) {
				if (inputPinB) {
					clearPinMetadata(inputPinB.get());
					dropIN("B");
					inputPinB.reset();
				}
				return;
			}

			createInputB();
		}

		static ConnectionType linkedOutputType(const InputPin& pin) {
			if (!pin || !pin->isConnected()) return ConnectionType::Adaptive;
			const auto link = pin->getLink().lock();
			if (!link) return ConnectionType::Adaptive;
			const ConnectionType type = pinType(link->left());
			return IsConcreteConnection(type) ? type : ConnectionType::Adaptive;
		}

		ConnectionType downstreamVectorType() {
			if (!outputPin || !outputPin->isConnected()) return ConnectionType::Adaptive;

			ImFlow::ImNodeFlow* flow = getHandler();
			if (!flow) return ConnectionType::Adaptive;

			ConnectionType bestType = ConnectionType::Adaptive;
			for (const auto& [_, node] : flow->getNodes()) {
				if (!node) continue;
				for (const auto& input : node->getIns()) {
					if (!input || !input->isConnected()) continue;
					const auto link = input->getLink().lock();
					if (!link || link->left() != outputPin.get()) continue;

					const ConnectionType type = pinType(input.get());
					if (!IsVectorConnection(type)) continue;
					if (!IsConcreteConnection(bestType) || ConnectionComponentCount(type) > ConnectionComponentCount(bestType))
						bestType = type;
				}
			}
			return bestType;
		}

		ConnectionType effectiveVectorType() {
			const ConnectionType aType = linkedOutputType(inputPinA);
			if (IsVectorConnection(aType)) return aType;

			if (!vectorOpIsSingleInput(_currentOp) && _currentOp != VectorOps::SCALE) {
				const ConnectionType bType = linkedOutputType(inputPinB);
				if (IsVectorConnection(bType)) return bType;
			}

			const ConnectionType outputType = downstreamVectorType();
			if (IsVectorConnection(outputType)) return outputType;

			return ConnectionType::Float3;
		}

		bool canConnectVectorInput(const ImFlow::Pin* out, bool isInputA) {
			const ConnectionType outType = pinType(out);
			if (_currentOp == VectorOps::SCALE && !isInputA)
				return outType == ConnectionType::Float
					|| (outType == ConnectionType::Adaptive && pinAllowsConcreteType(out, ConnectionType::Float));

			if (outType == ConnectionType::Adaptive) {
				if (_currentOp == VectorOps::CROSS)
					return pinAllowsConcreteType(out, ConnectionType::Float3);

				const ConnectionType otherType = isInputA ? linkedOutputType(inputPinB) : linkedOutputType(inputPinA);
				if (IsVectorConnection(otherType)) return pinAllowsConcreteType(out, otherType);

				const ConnectionType outputType = downstreamVectorType();
				if (IsVectorConnection(outputType)) return pinAllowsConcreteType(out, outputType);

				return pinAllowsConcreteType(out, ConnectionType::Float2)
					|| pinAllowsConcreteType(out, ConnectionType::Float3)
					|| pinAllowsConcreteType(out, ConnectionType::Float4);
			}

			if (!IsVectorConnection(outType)) return false;
			if (_currentOp == VectorOps::CROSS) return outType == ConnectionType::Float3;

			const ConnectionType otherType = isInputA ? linkedOutputType(inputPinB) : linkedOutputType(inputPinA);
			if (IsVectorConnection(otherType)) return vectorTypesMatch(outType, otherType);

			const ConnectionType outputType = downstreamVectorType();
			if (IsVectorConnection(outputType)) return vectorTypesMatch(outType, outputType);

			return true;
		}

		bool canUseOpWithCurrentLinks(VectorOps op) {
			const ConnectionType aType = linkedOutputType(inputPinA);
			const ConnectionType bType = linkedOutputType(inputPinB);
			const ConnectionType outType = downstreamVectorType();

			if (vectorOpIsSingleInput(op)) {
				if (IsVectorConnection(aType) && IsVectorConnection(outType) && aType != outType) return false;
				return true;
			}

			if (op == VectorOps::SCALE)
				return !IsConcreteConnection(bType) || bType == ConnectionType::Float;

			if (!vectorOpIsSingleInput(op) && IsConcreteConnection(bType) && !IsVectorConnection(bType))
				return false;

			if (op == VectorOps::CROSS) {
				return (!IsVectorConnection(aType) || aType == ConnectionType::Float3)
					&& (!IsVectorConnection(bType) || bType == ConnectionType::Float3)
					&& (!IsVectorConnection(outType) || outType == ConnectionType::Float3);
			}

			if (IsVectorConnection(aType) && IsVectorConnection(bType) && aType != bType) return false;
			if (IsVectorConnection(aType) && IsVectorConnection(outType) && aType != outType) return false;
			if (IsVectorConnection(bType) && IsVectorConnection(outType) && bType != outType) return false;
			return true;
		}

		void setPinVisualType(const InputPin& pin, ConnectionType type) const {
			setPinType(pin.get(), type);
			pin->getStyle()->color = ConnectionTypeColor(type);
		}

		void setPinVisualType(const OutputPin& pin, ConnectionType type) const {
			setPinType(pin.get(), type);
			pin->getStyle()->color = ConnectionTypeColor(type);
		}

		void refreshAdaptivePins() {
			syncInputBPin();

			if (!canUseOpWithCurrentLinks(_currentOp)) {
				for (uint8_t i = 0; i < static_cast<uint8_t>(VectorOps::kNUM); ++i) {
					const auto op = static_cast<VectorOps>(i);
					if (canUseOpWithCurrentLinks(op)) {
						_currentOp = op;
						break;
					}
				}
			}

			const ConnectionType type = effectiveVectorType();
			if (inputPinA->isConnected()) {
				const ConnectionType connectedType = linkedOutputType(inputPinA);
				setPinVisualType(inputPinA, IsVectorConnection(connectedType) ? connectedType : type);
			} else if (outputPin->isConnected() || (inputPinB && inputPinB->isConnected())) {
				setPinVisualType(inputPinA, type);
			} else {
				setPinVisualType(inputPinA, ConnectionType::Adaptive);
			}

			if (inputPinB) {
				if (_currentOp == VectorOps::SCALE) {
					setPinVisualType(inputPinB, ConnectionType::Float);
				} else if (inputPinB->isConnected()) {
					const ConnectionType connectedType = linkedOutputType(inputPinB);
					setPinVisualType(inputPinB, IsVectorConnection(connectedType) ? connectedType : type);
				} else if (outputPin->isConnected() || inputPinA->isConnected()) {
					setPinVisualType(inputPinB, type);
				} else {
					setPinVisualType(inputPinB, ConnectionType::Adaptive);
				}
			}

			if (outputPin->isConnected() || inputPinA->isConnected() || (inputPinB && inputPinB->isConnected())) {
				setPinVisualType(outputPin, type);
			} else {
				setPinVisualType(outputPin, ConnectionType::Adaptive);
			}
		}

		glm::vec4 vecA = {0, 0, 0, 0};
		glm::vec4 vecB = {0, 0, 0, 0};
		VectorOps _currentOp = VectorOps::ADD;
		InputPin inputPinA;
		InputPin inputPinB;
		OutputPin outputPin;
	};

	enum class MixOps : uint8_t {
		Lerp,
		Add,
		Subtract,
		Multiply,
		Screen,
		Divide,
		Difference,
		Darken,
		Lighten,
		Overly,
		ColorDodge,
		ColorBurn,
		Hue,
		Saturation,
		Value,
		Color,
		SoftLight,
		HardLight,

		kNUM
	};

	inline const char* MixOpToString(MixOps op) {
		switch (op) {
			case MixOps::Lerp:        return "Lerp";
			case MixOps::Add:         return "Add";
			case MixOps::Subtract:    return "Subtract";
			case MixOps::Multiply:    return "Multiply";
			case MixOps::Screen:      return "Screen";
			case MixOps::Divide:      return "Divide";
			case MixOps::Difference:  return "Difference";
			case MixOps::Darken:      return "Darken";
			case MixOps::Lighten:     return "Lighten";
			case MixOps::Overly:      return "Overly";
			case MixOps::ColorDodge:  return "ColorDodge";
			case MixOps::ColorBurn:   return "ColorBurn";
			case MixOps::Hue:         return "Hue";
			case MixOps::Saturation:  return "Saturation";
			case MixOps::Value:       return "Value";
			case MixOps::Color:       return "Color";
			case MixOps::SoftLight:   return "SoftLight";
			case MixOps::HardLight:   return "HardLight";
			default: assert(false && "Unhandled case in MixOpToString");
		}
	}

	struct MixNode : NodeWithCategory<NodeCategory::CONVERT>, NodeWithStaticVariable<float> {
		explicit MixNode() : NodeWithCategory("Mix") {
			inputPinA = addCodeIN("A", ConnectionType::Float3, "float3(0,0,0)");
			inputPinB = addCodeIN("B", ConnectionType::Float3, "float3(0,0,0)");
			addCodeOUT("Color", ConnectionType::Float3)->behaviour([this] {
				return emit();
			});
		}
		std::string emit() {
			auto& ctx = *GeneratorContext::active;
			const std::string outName = "node" + std::to_string(getUID()) + "_out";

			const std::string a = pullAs("A", ConnectionType::Float3);
			const std::string b = pullAs("B", ConnectionType::Float3);

			const std::string factorVarName = "push.user.val_" + std::to_string(getUID());
			ctx.body << "float3 " << outName <<  " = " << functionEmit() << "(" << a << ", " << b << ", " << factorVarName << ");\n";

			ctx.addPushConstant(this, geVariableType(), &factor);
			return outName;
		}

		void draw() override {
			ImGui::SetNextItemWidth(100.f);
			ImGui::DragFloat("Factor", &factor, 0.005f, 0.f, 1.f);
			ImGui::SetNextItemWidth(100.f);
			if (ImGui::BeginCombo("##VecOp", MixOpToString(_currentOp))) {
				for (int i = 0; i < static_cast<int>(MixOps::kNUM); i++) {
					const auto op = static_cast<MixOps>(i);
					const bool selected = _currentOp == op;
					if (ImGui::Selectable(MixOpToString(op), selected))
						_currentOp = op;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		float getValue() const override {
			return factor;
		}

		uint64_t stateHash() const override {
			return static_cast<uint64_t>(_currentOp);
		}
	private:
		std::string functionEmit() const {
			switch (_currentOp) {
				case MixOps::Lerp:       return "mix_lerp";
				case MixOps::Add:        return "mix_add";
				case MixOps::Subtract:   return "mix_subtract";
				case MixOps::Multiply:   return "mix_multiply";
				case MixOps::Screen:     return "mix_screen";
				case MixOps::Divide:     return "mix_divide";
				case MixOps::Difference: return "mix_difference";
				case MixOps::Darken:     return "mix_darken";
				case MixOps::Lighten:    return "mix_lighten";
				case MixOps::Overly:     return "mix_overly";
				case MixOps::ColorDodge: return "mix_colordodge";
				case MixOps::ColorBurn:  return "mix_colorburn";
				case MixOps::Hue:        return "mix_hue";
				case MixOps::Saturation: return "mix_saturation";
				case MixOps::Value:      return "mix_value";
				case MixOps::Color:      return "mix_color";
				case MixOps::SoftLight:  return "mix_softlight";
				case MixOps::HardLight:  return "mix_hardlight";
				default: assert(false && "Unhandled case in emit for MixNode");
			}
		}

		float factor = 0.f;
		MixOps _currentOp = MixOps::Lerp;
		InputPin inputPinA;
		InputPin inputPinB;
	};
}
