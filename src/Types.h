//
// Created by Hayden Rivas on 5/23/26.
//

#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

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

	enum class VariableType {
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

	// how we describe the input in the struct 'UserData'
	static const char* VariableTypeToCodegenString(VariableType type) {
		switch (type) {
			case VariableType::Float: return "float";
			case VariableType::Float2: return "float2";
			case VariableType::Float3: return "float3";
			case VariableType::Float4: return "float4";
			case VariableType::Int8:   return "int8_t";
			case VariableType::Int16:  return "int16_t";
			case VariableType::Int32:  return "int";
			case VariableType::Int64:  return "int64_t";
			case VariableType::UInt8:  return "uint8_t";
			case VariableType::UInt16: return "uint16_t";
			case VariableType::UInt32: return "uint";
			case VariableType::UInt64: return "uint64_t";
			case VariableType::Double: return "double";
			case VariableType::Bool:   return "bool";
			case VariableType::Texture2D: return "DescriptorHandle<Texture2D>";
			default: assert(false && "Unhanded case in StringFromPCType");
		}
	}

	// forward declare this guy
	struct INode;
	// how we represent every input + textures
	struct VariableEntry {
		std::string varName;
		VariableType type;
		size_t offset;
		size_t size;
		// points at the node-owned value; bytes are copied into the user buffer at upload time
		const void* src;
	};


	enum class ConnectionType : uint8_t {
		Float,
		Float2,
		Float3,
		Float4,
		Adaptive,
	};

	// how we describe the connection in the body
	inline const char* ConnectionTypeToString(ConnectionType t) {
		switch (t) {
			case ConnectionType::Float: return "float";
			case ConnectionType::Float2:  return "float2";
			case ConnectionType::Float3:  return "float3";
			case ConnectionType::Float4:  return "float4";
			case ConnectionType::Adaptive:
				assert(false && "Adaptive is not a concrete shader type");
			default:
				assert(false && "Unhandled case in ShaderTypeToString");
		}
	}

	inline bool IsConcreteConnection(ConnectionType t) {
		return t != ConnectionType::Adaptive;
	}

	inline bool IsVectorConnection(ConnectionType t) {
		return t == ConnectionType::Float2 || t == ConnectionType::Float3 || t == ConnectionType::Float4;
	}

	inline uint8_t ConnectionComponentCount(ConnectionType t) {
		switch (t) {
			case ConnectionType::Float: return 1;
			case ConnectionType::Float2: return 2;
			case ConnectionType::Float3: return 3;
			case ConnectionType::Float4: return 4;
			case ConnectionType::Adaptive: return 4;
			default: assert(false && "Unhandled case in ConnectionComponentCount");
		}
	}

	// horribly architecture to get to this point but ill stick with it for a little bit
	inline ConnectionType VariableTypeToConnectionTypeEXT(VariableType type) {
		switch (type) {
			case VariableType::Float: return ConnectionType::Float;
			case VariableType::Float2: return ConnectionType::Float2;
			case VariableType::Float3: return ConnectionType::Float3;
			case VariableType::Float4: return ConnectionType::Float4;
			default: assert(false && "Unhandled case in VariableTypeToConnectionTypeEXT!");
		}
	}
	inline VariableType ConnectionTypeToVariableTypeEXT(ConnectionType type) {
		switch (type) {
			case ConnectionType::Float: return VariableType::Float;
			case ConnectionType::Float2: return VariableType::Float2;
			case ConnectionType::Float3: return VariableType::Float3;
			case ConnectionType::Float4: return VariableType::Float4;
			case ConnectionType::Adaptive: assert(false && "Adaptive cannot be converted to a variable type");
			default: assert(false && "Unhandled case in ConnectionTypeToVariableTypeEXT!");
		}
	}

	inline std::unordered_map<const ImFlow::Pin*, ConnectionType>& pinTypeRegistry() {
		static std::unordered_map<const ImFlow::Pin*, ConnectionType> reg;
		return reg;
	}
	inline std::unordered_map<const ImFlow::Pin*, std::vector<ConnectionType>>& pinConcreteOptionsRegistry() {
		static std::unordered_map<const ImFlow::Pin*, std::vector<ConnectionType>> reg;
		return reg;
	}
	inline void setPinType(const ImFlow::Pin* pin, ConnectionType t) { pinTypeRegistry()[pin] = t; }
	inline void setPinConcreteOptions(const ImFlow::Pin* pin, std::vector<ConnectionType> options) {
		pinConcreteOptionsRegistry()[pin] = std::move(options);
	}
	inline void clearPinMetadata(const ImFlow::Pin* pin) {
		pinTypeRegistry().erase(pin);
		pinConcreteOptionsRegistry().erase(pin);
	}
	inline ConnectionType pinType(const ImFlow::Pin* pin) {
		const auto it = pinTypeRegistry().find(pin);
		return it != pinTypeRegistry().end() ? it->second : ConnectionType::Float;
	}
	inline bool pinAllowsConcreteType(const ImFlow::Pin* pin, ConnectionType type) {
		const auto it = pinConcreteOptionsRegistry().find(pin);
		if (it == pinConcreteOptionsRegistry().end() || it->second.empty()) return true;
		return std::ranges::find(it->second, type) != it->second.end();
	}

	// defines allowed behavior from two types
	inline bool CanConvertConnection(ConnectionType from, ConnectionType to) {
		if (from == ConnectionType::Adaptive || to == ConnectionType::Adaptive) return true;
		if (from == to) return true;
		// a scalar type to its associated vector represetnation is always supported
		if (from == ConnectionType::Float && to == ConnectionType::Float2) return true;
		if (from == ConnectionType::Float && to == ConnectionType::Float3) return true;
		if (from == ConnectionType::Float && to == ConnectionType::Float4) return true;
		return false;
	}

	inline std::string ConvertConnectionExpression(std::string expr, ConnectionType from, ConnectionType to) {
		if (from == ConnectionType::Adaptive) {
			return expr;
		}
		if (to == ConnectionType::Adaptive) {
			assert(false && "Adaptive cannot be used as a codegen target type");
		}
		if (from == to) return expr;
		assert(CanConvertConnection(from, to) && "Unsupported connection conversion");

		if (from == ConnectionType::Float && to != ConnectionType::Float)
			return std::string(ConnectionTypeToString(to)) + "(" + expr + ")";

		return expr;
	}

	inline bool CanConnectPins(const ImFlow::Pin* out, const ImFlow::Pin* in) {
		const ConnectionType from = pinType(out);
		const ConnectionType to = pinType(in);

		if (from == ConnectionType::Adaptive && to == ConnectionType::Adaptive) {
			const auto outIt = pinConcreteOptionsRegistry().find(out);
			const auto inIt = pinConcreteOptionsRegistry().find(in);
			if (outIt == pinConcreteOptionsRegistry().end() || outIt->second.empty()) return true;
			if (inIt == pinConcreteOptionsRegistry().end() || inIt->second.empty()) return true;
			return std::ranges::any_of(outIt->second, [&](ConnectionType type) {
				return std::ranges::find(inIt->second, type) != inIt->second.end();
			});
		}
		if (from == ConnectionType::Adaptive)
			return IsConcreteConnection(to) && pinAllowsConcreteType(out, to);
		if (to == ConnectionType::Adaptive)
			return IsConcreteConnection(from) && pinAllowsConcreteType(in, from);
		return CanConvertConnection(from, to);
	}

	// filters similar to provided ones, but for our use case
	// you shouldnt need to call either of these yourself
	inline std::function<bool(ImFlow::Pin*, ImFlow::Pin*)> CodeFilterCompatible() {
		return [](const ImFlow::Pin* out, const ImFlow::Pin* in) {
			return CanConnectPins(out, in);
		};
	}
	inline std::function<bool(ImFlow::Pin*, ImFlow::Pin*)> CodeFilterExact() {
		return [](const ImFlow::Pin* out, const ImFlow::Pin* in) {
			return pinType(out) == pinType(in);
		};
	}

	// shared state during generation
	struct GeneratorContext {
		std::stringstream body;
		std::vector<VariableEntry> pushConstants;
		size_t pcSize = 0;

		inline static GeneratorContext* active = nullptr;

		struct Scope {
			explicit Scope(GeneratorContext* ctx) { active = ctx; }
			~Scope() { active = nullptr; }
			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;
		};

		// src points at the value owned by the node instance to upload each frame
		void addPushConstant(INode* node, VariableType type, const void* src, const std::string& slot = {});
	};

}
