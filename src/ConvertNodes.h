//
// Created by Hayden Rivas on 5/24/26.
//

#pragma once

#include "NodeBasics.h"
#include "glm/glm.hpp"

namespace Andesite {

	struct SeperateXYZ : NodeWithCategory<NodeCategory::CONVERT> {
		explicit SeperateXYZ() {
			setTitle("Seperate XYZ");
			inputPin = addIN<glm::vec3>("Input", {}, FloatOrVec3Filter(), vectorPinStyle());

			addOUT<float>("X", scalarPinStyle())->behaviour([this] { return getInVal<glm::vec3>("Input").x; });
			addOUT<float>("Y", scalarPinStyle())->behaviour([this] { return getInVal<glm::vec3>("Input").y; });
			addOUT<float>("Z", scalarPinStyle())->behaviour([this] { return getInVal<glm::vec3>("Input").z; });
		}
		void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
			const std::string outVarX = "node" + std::to_string(getUID()) + "_outX";
			const std::string outVarY = "node" + std::to_string(getUID()) + "_outY";
			const std::string outVarZ = "node" + std::to_string(getUID()) + "_outZ";

			const std::string inVar = inputPin->isConnected() ? ctx.resolveUpstreamAs(inputPin, "float3") : "float3(0,0,0)";
			stream << "float " << outVarX << " = " << inVar << ".x;\n";
			stream << "float " << outVarY << " = " << inVar << ".y;\n";
			stream << "float " << outVarZ << " = " << inVar << ".z;\n";

			ctx.registerOutput(getUID(), 0, outVarX, "float");
			ctx.registerOutput(getUID(), 1, outVarY, "float");
			ctx.registerOutput(getUID(), 2, outVarZ, "float");
		}
	private:
    	std::shared_ptr<ImFlow::InPin<glm::vec3>> inputPin;
	};

	struct CombineXYZ : NodeWithCategory<NodeCategory::CONVERT> {
		explicit CombineXYZ() {
			setTitle("Combine XYZ");

			inputPinX = addIN<float>("X", 0, ImFlow::ConnectionFilter::Numbers(), scalarPinStyle());
			inputPinY = addIN<float>("Y", 0, ImFlow::ConnectionFilter::Numbers(), scalarPinStyle());
			inputPinZ = addIN<float>("Z", 0, ImFlow::ConnectionFilter::Numbers(), scalarPinStyle());

			addOUT<glm::vec3>("Output", vectorPinStyle())->behaviour([this] {
				return glm::vec3(getInVal<float>("X"), getInVal<float>("Y"), getInVal<float>("Z"));
			});
		}
		void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
			const std::string outVar = "node" + std::to_string(getUID()) + "_out";
			const std::string x = inputPinX->isConnected() ? ctx.resolveUpstream(inputPinX) : "0.0";
			const std::string y = inputPinY->isConnected() ? ctx.resolveUpstream(inputPinY) : "0.0";
			const std::string z = inputPinZ->isConnected() ? ctx.resolveUpstream(inputPinZ) : "0.0";
			stream << "float3 " << outVar << " = float3(" << x << ", " << y << ", " << z << ");\n";
			ctx.registerOutput(getUID(), 0, outVar, "float3");
		}
	private:
    	std::shared_ptr<ImFlow::InPin<float>> inputPinX;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinY;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinZ;
	};

	enum class Operators : uint8_t {
		ADD,
		SUBTRACT,
		MULTIPLY,
		DIVIDE,

		kNUM_OPERATIONS
	};
	static const char* operatorToString(Operators op) {
		switch (op) {
			case Operators::ADD: return "Add";
			case Operators::SUBTRACT: return "Subtract";
			case Operators::MULTIPLY: return "Multiply";
			case Operators::DIVIDE: return "Divide";
			default: assert(false);
		}
	}

    struct MathNode : NodeWithCategory<NodeCategory::CONVERT> {
        explicit MathNode() {
            setTitle("Math");
            setStyle(NodeStyleFromCategory(NodeCategory::CONVERT));

            inputPinA = addIN<float>("Input 1", 0, ImFlow::ConnectionFilter::SameType(), scalarPinStyle());
            inputPinB = addIN<float>("Input 2", 0, ImFlow::ConnectionFilter::SameType(), scalarPinStyle());
            addOUT<float>("Out", scalarPinStyle())->behaviour([this] {
                const float a = getInVal<float>("A");
                const float b = getInVal<float>("B");
                switch (currentOperation) {
                    case Operators::ADD: return a + b;
                    case Operators::SUBTRACT: return a - b;
                    case Operators::MULTIPLY: return a * b;
                    case Operators::DIVIDE: return a / b;
                	default: assert(false);
                }
            });
        }
    	void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
	        const std::string outVar = "node" + std::to_string(this->getUID()) + "_out";
        	const std::string inputA = inputPinA->isConnected()
        		? ctx.resolveUpstream(inputPinA)
				: "0.0";

        	const std::string inputB = inputPinB->isConnected()
        		? ctx.resolveUpstream(inputPinB)
        		: "0.0";

        	stream << "float " << outVar << " = " << inputA << opSignToString() << inputB << ";\n";
        	ctx.registerOutput(getUID(), 0, outVar, "float");

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
            if (ImGui::BeginCombo("###Options", operatorToString(currentOperation))) {
                for (int i = 0; i < static_cast<int>(Operators::kNUM_OPERATIONS); i++) {
                    const auto op = static_cast<Operators>(i);
                    const bool isSelected = (static_cast<int>(currentOperation) == i);
                    if (ImGui::Selectable(operatorToString(op), isSelected)) {
                        currentOperation = op;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
		Operators getOp() const {
	        return currentOperation;
        }
    	uint64_t stateHash() const override {
	        return static_cast<uint64_t>(currentOperation);
        }
    private:
    	std::string opSignToString() const {
    		switch (currentOperation) {
    			case Operators::ADD: return "+";
    			case Operators::SUBTRACT: return "-";
    			case Operators::MULTIPLY: return "*";
    			case Operators::DIVIDE: return "/";
    			default: assert(false && "Unhandled Operators to std::string");
    		}
    	}
		float valA = 0.f;
		float valB = 0.f;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinA;
    	std::shared_ptr<ImFlow::InPin<float>> inputPinB;
        Operators currentOperation = Operators::MULTIPLY;
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
		kNUM_OPERATIONS
	};
	static const char* vectorOpToString(VectorOps op) {
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
			default: assert(false);
		}
	}

	// ops that only use A (no B pin needed)
	static bool vectorOpIsSingleInput(VectorOps op) {
		return op == VectorOps::NORMALIZE || op == VectorOps::LENGTH;
	}

	struct VectorMathNode : NodeWithCategory<NodeCategory::CONVERT> {
		explicit VectorMathNode() {
			setTitle("Vector Math");
			inputPinA = addIN<glm::vec3>("A", {}, FloatOrVec3Filter(), vectorPinStyle());
			inputPinB = addIN<glm::vec3>("B", {}, FloatOrVec3Filter(), vectorPinStyle());
			addOUT<glm::vec3>("Out", vectorPinStyle())->behaviour([this] {
				return getInVal<glm::vec3>("A");
			});
		}

		void emitSource(std::stringstream& stream, GeneratorContext& ctx) override {
			const std::string uid    = std::to_string(getUID());
			const std::string vecOut = "node" + uid + "_vec";

			const std::string a = inputPinA->isConnected()
				? ctx.resolveUpstreamAs(inputPinA, "float3")
				: "float3(0,0,0)";

			// B resolution: SCALE expects a scalar, everything else vec3
			const bool needsB = !vectorOpIsSingleInput(currentOp);
			const std::string b = [&]() -> std::string {
				if (!needsB) return "";
				if (!inputPinB->isConnected())
					return currentOp == VectorOps::SCALE ? "1.0" : "float3(0,0,0)";
				return currentOp == VectorOps::SCALE
					? ctx.resolveUpstreamAs(inputPinB, "float")
					: ctx.resolveUpstreamAs(inputPinB, "float3");
			}();

			// scalar ops broadcast into float3 so the single output is always vec3
			if (currentOp == VectorOps::DOT) {
				stream << "float3 " << vecOut << " = float3(dot(" << a << ", " << b << "));\n";
			} else if (currentOp == VectorOps::LENGTH) {
				stream << "float3 " << vecOut << " = float3(length(" << a << "));\n";
			} else {
				switch (currentOp) {
					case VectorOps::ADD:       stream << "float3 " << vecOut << " = " << a << " + " << b << ";\n"; break;
					case VectorOps::SUBTRACT:  stream << "float3 " << vecOut << " = " << a << " - " << b << ";\n"; break;
					case VectorOps::MULTIPLY:  stream << "float3 " << vecOut << " = " << a << " * " << b << ";\n"; break;
					case VectorOps::SCALE:     stream << "float3 " << vecOut << " = " << a << " * " << b << ";\n"; break;
					case VectorOps::CROSS:     stream << "float3 " << vecOut << " = cross(" << a << ", " << b << ");\n"; break;
					case VectorOps::NORMALIZE: stream << "float3 " << vecOut << " = normalize(" << a << ");\n"; break;
					case VectorOps::REFLECT:   stream << "float3 " << vecOut << " = reflect(" << a << ", " << b << ");\n"; break;
					default: assert(false);
				}
			}
			ctx.registerOutput(getUID(), 0, vecOut, "float3");
		}

		void draw() override {
			ImGui::SetNextItemWidth(150.f);
			if (ImGui::BeginCombo("##VecOp", vectorOpToString(currentOp))) {
				for (int i = 0; i < static_cast<int>(VectorOps::kNUM_OPERATIONS); i++) {
					const auto op = static_cast<VectorOps>(i);
					const bool selected = currentOp == op;
					if (ImGui::Selectable(vectorOpToString(op), selected))
						currentOp = op;
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		uint64_t stateHash() const override {
			return static_cast<uint64_t>(currentOp);
		}

	private:
		std::shared_ptr<ImFlow::InPin<glm::vec3>> inputPinA;
		std::shared_ptr<ImFlow::InPin<glm::vec3>> inputPinB;
		VectorOps currentOp = VectorOps::ADD;
	};
}
