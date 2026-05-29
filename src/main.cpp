#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <mythril/CTX.h>
#include <mythril/CTXBuilder.h>
#include <mythril/RenderGraphBuilder.h>

#include <chrono>
#include <cctype>
#include <fstream>
#include <functional>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <ranges>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#ifdef __APPLE__
#include "MacOSGestures.h"
#endif

#include "Compiler.h"
#include "NodeBasics.h"
#include "ShaderNodes.h"
#include "ConvertNodes.h"
#include "InputNodes.h"
#include "TextureNodes.h"
#include "UniqueNodes.h"

namespace Andesite {
	namespace GPU {
		struct Camera {
			glm::mat4 proj;
			glm::mat4 view;
			glm::vec3 position;
		};

		struct FrameData {
			Camera camera;
			float time;
			glm::vec2 resolution;
		};

		struct PushConstant {
			glm::mat4 model;
			VkDeviceAddress vba;
			VkDeviceAddress frame;
			uint64_t linearSampler;
			uint64_t nearestSampler;
			VkDeviceAddress user;
		};
	} // namespace GPU
	struct CPUCamera {
		glm::vec3 position;
		float aspectRatio;
		float fov;
		float nearPlane;
		float farPlane;
	};

	struct Vertex {
		alignas(16) glm::vec3 position;
		float uv_x;
		alignas(16) glm::vec3 normal;
		float uv_y;
		alignas(16) glm::vec4 tangent;

		Vertex() = default;
		Vertex(glm::vec3 pos) :
			position(pos) {};
		Vertex(glm::vec3 pos, glm::vec3 norm, glm::vec2 uv) :
			position(pos), normal(norm), uv_x(uv.x), uv_y(uv.y) {};
	};

	const std::vector<Vertex> cubeVertices = {
			// front face
			{{-1.f, -1.f, -1.f}, {0.f, 0.f, -1.f}, {0.0f, 0.0f}}, // A 0
			{{1.f, -1.f, -1.f}, {0.f, 0.f, -1.f}, {1.0f, 0.0f}}, // B 1
			{{1.f, 1.f, -1.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f}}, // C 2
			{{-1.f, 1.f, -1.f}, {0.f, 0.f, -1.f}, {0.0f, 1.0f}}, // D 3

			// back face
			{{-1.f, -1.f, 1.f}, {0.f, 0.f, 1.f}, {0.0f, 0.0f}}, // E 4
			{{1.f, -1.f, 1.f}, {0.f, 0.f, 1.f}, {1.0f, 0.0f}}, // F 5
			{{1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f}}, // G 6
			{{-1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {0.0f, 1.0f}}, // H 7

			// left face
			{{-1.f, 1.f, -1.f}, {-1.f, 0.f, 0.f}, {0.0f, 1.0f}}, // D 8
			{{-1.f, -1.f, -1.f}, {-1.f, 0.f, 0.f}, {0.0f, 0.0f}}, // A 9
			{{-1.f, -1.f, 1.f}, {-1.f, 0.f, 0.f}, {1.0f, 0.0f}}, // E 10
			{{-1.f, 1.f, 1.f}, {-1.f, 0.f, 0.f}, {1.0f, 1.0f}}, // H 11

			// right face
			{{1.f, -1.f, -1.f}, {1.f, 0.f, 0.f}, {0.0f, 0.0f}}, // B 12
			{{1.f, 1.f, -1.f}, {1.f, 0.f, 0.f}, {0.0f, 1.0f}}, // C 13
			{{1.f, 1.f, 1.f}, {1.f, 0.f, 0.f}, {1.0f, 1.0f}}, // G 14
			{{1.f, -1.f, 1.f}, {1.f, 0.f, 0.f}, {1.0f, 0.0f}}, // F 15

			// bottom face
			{{-1.f, -1.f, -1.f}, {0.f, -1.f, 0.f}, {0.0f, 1.0f}}, // A 16
			{{1.f, -1.f, -1.f}, {0.f, -1.f, 0.f}, {1.0f, 1.0f}}, // B 17
			{{1.f, -1.f, 1.f}, {0.f, -1.f, 0.f}, {1.0f, 0.0f}}, // F 18
			{{-1.f, -1.f, 1.f}, {0.f, -1.f, 0.f}, {0.0f, 0.0f}}, // E 19

			// top face
			{{1.f, 1.f, -1.f}, {0.f, 1.f, 0.f}, {1.0f, 1.0f}}, // C 20
			{{-1.f, 1.f, -1.f}, {0.f, 1.f, 0.f}, {0.0f, 1.0f}}, // D 21
			{{-1.f, 1.f, 1.f}, {0.f, 1.f, 0.f}, {0.0f, 0.0f}}, // H 22
			{{1.f, 1.f, 1.f}, {0.f, 1.f, 0.f}, {1.0f, 0.0f}}, // G 23
	};

	const std::vector<uint32_t> cubeIndices = {
			// front and back
			0, 3, 2, 2, 1, 0, 4, 5, 6, 6, 7, 4,
			// left and right
			11, 8, 9, 9, 10, 11, 12, 13, 14, 14, 15, 12,
			// bottom and top
			16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

	std::unordered_map<NodeCategory, std::vector<NodeRegistry::NodeDefinition> >
	NodeRegistry::categorizedNodes;

	glm::mat4 calculateViewMatrix(const CPUCamera& camera) {
		return glm::lookAt(camera.position,
		                   camera.position + glm::vec3(0, 0, -1),
		                   glm::vec3(0, 1, 0));
	}
	glm::mat4 calculateProjectionMatrix(const CPUCamera& camera) {
		return glm::perspective(glm::radians(camera.fov),
		                        camera.aspectRatio,
		                        camera.nearPlane,
		                        camera.farPlane);
	}

	inline SDL_Window* BuildSDLWindow(bool isResizable) {
		const bool sdl_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
		assert(sdl_initialized && "SDL could not be initialized!");
		constexpr SDL_WindowFlags required_window_flags = SDL_WINDOW_VULKAN;
		SDL_Window* sdlWindow =
				SDL_CreateWindow("My Custom Window",
				                 1280,
				                 720,
				                 required_window_flags | SDL_WINDOW_HIGH_PIXEL_DENSITY |
				                 (isResizable ? SDL_WINDOW_RESIZABLE : 0));
		assert(sdlWindow != nullptr && "SDL window could not be created!");
		return sdlWindow;
	}
	inline void DestroySDLWindow(SDL_Window* sdlWindow) {
		SDL_DestroyWindow(sdlWindow);
		SDL_Quit();
	}

	struct Dims {
		uint32_t width;
		uint32_t height;
	};

	inline Dims GetSDLWindowFramebufferSize(SDL_Window* sdlWindow) {
		int w, h;
		SDL_GetWindowSizeInPixels(sdlWindow, &w, &h);
		return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	}

	void n_main() {
		static constexpr VkFormat kOffscreenFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

		static const std::filesystem::path kAssetsDir = "../../assets/";
		static const std::vector<std::string> slang_searchpaths = {
				(kAssetsDir / "shaders/").string(),
		};

		SDL_Window* sdlWindow = BuildSDLWindow(true);
		const auto& initialWindowSize = GetSDLWindowFramebufferSize(sdlWindow);

		auto ctx =
				mythril::CTXBuilder{}
				.set_vulkan_cfg({
					.app_name = "Cool App Name",
					.engine_name = "Cool Engine Name",
				})
				.set_slang_cfg({.searchpaths = slang_searchpaths})
				.set_window_surface([sdlWindow](VkInstance instance) {
						VkSurfaceKHR surface;
						SDL_Vulkan_CreateSurface(sdlWindow, instance, nullptr, &surface);
						return surface;
					},
					[](VkInstance instance, VkSurfaceKHR surface_khr) {
						SDL_Vulkan_DestroySurface(instance, surface_khr, nullptr);
					})
				.with_default_swapchain({
					.width = initialWindowSize.width,
					.height = initialWindowSize.height
				})
				.with_ImGui({
					.windowInitFunction = [sdlWindow] { ImGui_ImplSDL3_InitForVulkan(sdlWindow); },
					.windowDestroyFunction = [] { ImGui_ImplSDL3_Shutdown(); }})
				.build();

		const mythril::Dimensions dims = {
			initialWindowSize.width,
			initialWindowSize.height,
			1
		};
		mythril::Texture screenTexture = ctx->createTexture({
				.dimension = dims,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.usage = mythril::TextureUsageBits::TextureUsageBits_Attachment,
				.storage = mythril::StorageType::Device,
				.debugName = "Screen Texture"
		});

		mythril::Texture previewTexture = ctx->createTexture({
				.dimension = dims,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.usage = mythril::TextureUsageBits::TextureUsageBits_Attachment | mythril::TextureUsageBits_Sampled,
				.storage = mythril::StorageType::Device,
				.debugName = "Preview Color Texture",
		});
		mythril::Texture previewDepthTexture = ctx->createTexture({
			.dimension = dims,
			.format = VK_FORMAT_D16_UNORM,
			.usage = mythril::TextureUsageBits_Attachment,
			.storage = mythril::StorageType::Memoryless,
			.debugName = "Preview Depth Texture",
		});

		// default samplers provided to user
		mythril::Sampler linearSampler = ctx->createSampler({
			.magFilter = mythril::SamplerFilter::Linear,
			.minFilter = mythril::SamplerFilter::Linear,
			.debugName = "Linear Sampler"
		});
		mythril::Sampler nearestSampler = ctx->createSampler({
			.magFilter = mythril::SamplerFilter::Nearest,
			.minFilter = mythril::SamplerFilter::Nearest,
			.debugName = "Nearest Sampler"
		});

		mythril::Shader defaultShader = ctx->createShader({
			.source = kAssetsDir / "shaders/DiffuseShader.slang",
			.debugName = "Diffuse Shader"
		});

		mythril::GraphicsPipeline mainPipeline = ctx->createGraphicsPipeline({
			.vertexShader = {defaultShader.handle(), "vs_main"},
			.fragmentShader = {defaultShader.handle(), "fs_main"},
			.topology = mythril::TopologyMode::TRIANGLE,
			.polygon = mythril::PolygonMode::FILL,
			.blend = mythril::BlendingMode::OFF,
			.cull = mythril::CullMode::BACK,
			.multisample = mythril::SampleCount::X1,
			.debugName = "Main Pipeline"
		});

		mythril::Buffer cubeVertexBuffer = ctx->createBuffer({
			.size = sizeof(Vertex) * cubeVertices.size(),
			.usage = mythril::BufferUsageBits::BufferUsageBits_Storage,
			.storage = mythril::StorageType::Device,
			.initialData = cubeVertices.data(),
			.debugName = "Cube Vertex Buffer"
		});
		mythril::Buffer cubeIndexBuffer = ctx->createBuffer({
			.size = sizeof(uint32_t) * cubeIndices.size(),
			.usage = mythril::BufferUsageBits::BufferUsageBits_Index,
			.storage = mythril::StorageType::Device,
			.initialData = cubeIndices.data(),
			.debugName = "Cube Index Buffer"
		});
		mythril::Buffer perFrameBuffer = ctx->createBuffer({
			.size = sizeof(GPU::FrameData),
			.usage = mythril::BufferUsageBits::BufferUsageBits_Storage,
			.storage = mythril::StorageType::Device,
			.debugName = "Frame Data Buffer"
		});

		auto startTime = std::chrono::high_resolution_clock::now();

		mythril::Dimensions swapDims = ctx->getSwapchainDimensions();
		CPUCamera camera = {
			.position = {0.f, 0.f, 5.f},
			.aspectRatio = static_cast<float>(swapDims.width) / static_cast<float>(swapDims.height),
			.fov = 80.f,
			.nearPlane = 0.1f,
			.farPlane = 50.f
		};


		// arbitrary starting size
		static constexpr auto defaultColor = glm::vec4(1, 0, 1, 1);
		mythril::Buffer userBuffer = ctx->createBuffer({
			.size = sizeof(defaultColor),
			.usage = mythril::BufferUsageBits::BufferUsageBits_Storage,
			.storage = mythril::StorageType::Device,
			.initialData = reinterpret_cast<const float*>(&defaultColor),
			.debugName = "User Buffer"
		});

		mythril::RenderGraph graph;
		graph.addGraphicsPass("geometry")
		.attachment({
			.texDesc = previewTexture,
			.clearValue = mythril::ClearValue::color(0.2f, 0.2f, 0.2f, 1.f),
			.loadOp = mythril::LoadOp::CLEAR,
			.storeOp = mythril::StoreOp::STORE
		})
		.attachment({
			.texDesc = previewDepthTexture,
			.clearValue = mythril::ClearValue::depth(1.f, 0),
			.loadOp = mythril::LoadOp::CLEAR,
			.storeOp = mythril::StoreOp::DONT_CARE
		})
		.dependency(perFrameBuffer, mythril::BufferAccess::ShaderRead)
		.dependency(userBuffer, mythril::BufferAccess::ShaderRead)
		.execute([&](mythril::CommandBuffer& cmd) {
			cmd.cmdBindGraphicsPipeline(mainPipeline);
			cmd.cmdBindDepthState({mythril::CompareOp::LessEqual, true});

			// rotating cube!
			const auto currentTime = std::chrono::high_resolution_clock::now();
			const float time = std::chrono::duration<float>(currentTime - startTime).count();

			glm::mat4 model = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 1.0f, 0.0f));
			model = glm::rotate(model, time * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));

			const GPU::PushConstant constants = {
				.model = model,
				.vba = cubeVertexBuffer.gpuAddress(),
				.frame = perFrameBuffer.gpuAddress(),
				.linearSampler = linearSampler.index(),
				.nearestSampler = nearestSampler.index(),
				.user = userBuffer.gpuAddress(),
			};
			cmd.cmdPushConstants(constants);
			cmd.cmdBindIndexBuffer(cubeIndexBuffer);
			cmd.cmdDrawIndexed(cubeIndices.size());
		});
		graph.addGraphicsPass("gui")
				.dependency(previewTexture, mythril::Layout::READ)
				.attachment({
					.texDesc = screenTexture,
					.clearValue = mythril::ClearValue::color(0.f, 0.f, 0.f, 1.f),
					.loadOp = mythril::LoadOp::CLEAR,
					.storeOp = mythril::StoreOp::STORE
				})
				.execute([](mythril::CommandBuffer& cmd) { cmd.cmdDrawImGui(); });
		graph.addIntermediate("present")
				.blit(screenTexture, ctx->getBackBufferTexture())
				.finish();
		graph.compile(*ctx);

		ImFontConfig fontConfig = {};
		fontConfig.FontDataOwnedByAtlas = false;
		fontConfig.OversampleV = 2;
		fontConfig.OversampleH = 1;
#ifdef __APPLE__
		// necessary for ImNodeFlow to render our font crisp
		fontConfig.RasterizerDensity = 2.f;
#endif

		ImGuiIO& io = ImGui::GetIO();
		ImFont* regularFont = io.Fonts->AddFontFromFileTTF((kAssetsDir / "fonts/NotoSans-Regular.ttf").c_str(), 17, &fontConfig);
		ImFont* boldFont = io.Fonts->AddFontFromFileTTF((kAssetsDir / "fonts/NotoSans-Bold.ttf").c_str(), 17, &fontConfig);

		ImFlow::ImNodeFlow f;
		f.getStyle().link_gradient_blend = 2.f;
		f.getGrid().config().extra_viewport_window_flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;

#ifdef __APPLE__
		struct {
			ImVec2 panVelocity  = {0.f, 0.f};
			float  zoomVelocity = 0.f;
			float  lastPinchCx  = 0.f;
			float  lastPinchCy  = 0.f;
		} gs;
		// per frame velocity settings (0=instant stop, 1=never stops)
		static constexpr float kPanSpeed = 0.35f;
		static constexpr float kZoomSpeed = 0.20f;
		static constexpr float kPanFriction  = 0.78f;
		static constexpr float kZoomFriction = 0.78f;

		Andesite::RegisterMacOSGestures(sdlWindow, {
			.onMagnify = [&gs](float magnification, float cx, float cy) {
				gs.lastPinchCx   = cx;
				gs.lastPinchCy   = cy;
				gs.zoomVelocity += magnification * kZoomSpeed;
			},
			.onPan = [&f, &gs](float dx, float dy) -> bool {
				if (!f.getGrid().hovered()) return false;
				const float scale    = f.getGrid().scale();
				gs.panVelocity.x    += dx * kPanSpeed / scale;
				gs.panVelocity.y    += dy * kPanSpeed / scale;
				return true;
			}
		});
#endif

		f.addNode<ScalarNode>({50, 100});
		f.addNode<MathNode>({250, 100});
		{
			auto rgbNode = f.addNode<RGBNode>({200, 300});
			auto pbrNode = f.addNode<PBRNode>({500, 300});
			auto output1 = f.addNode<OutputNode>({800, 350});
			rgbNode->getOuts()[0]->createLink(pbrNode->getIns()[0].get());
			pbrNode->getOuts()[0]->createLink(output1->getIns()[0].get());
		}

		auto findActiveOutputNode = [&f]() -> OutputNode* {
			OutputNode* firstOutput = nullptr;
			for (auto& node : f.getNodes() | std::views::values) {
				if (!node || node->toDestroy()) {
					continue;
				}
				auto* outputNode = dynamic_cast<OutputNode*>(node.get());
				if (!outputNode) {
					continue;
				}
				if (!firstOutput) {
					firstOutput = outputNode;
				}
				if (outputNode->hasShaderInputConnected()) {
					return outputNode;
				}
			}
			return firstOutput;
		};

		// todo: resolve name automatically from setTitle
		NodeRegistry::RegisterNode<ScalarNode>("Value Node", NodeCategory::INPUT);
		NodeRegistry::RegisterNode<RGBNode>("RGB Node", NodeCategory::INPUT);
		NodeRegistry::RegisterNode<MathNode>("Math Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<VectorMathNode>("Vector Math Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<TextureNode>("Texture Node", NodeCategory::TEXTURE);
		NodeRegistry::RegisterNode<SeperateComponentsNode>("Seperate Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<CombineComponentsNode>("Combine Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<PBRNode>("PBR Node", NodeCategory::SHADER);
		NodeRegistry::RegisterNode<OutputNode>("Output Node", NodeCategory::UNIQUE);
		NodeRegistry::RegisterNode<MixNode>("Mix Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<TextureCoordinateNode>("Texture Coordinate", NodeCategory::INPUT);

		f.rightClickPopUpContent([&f](ImFlow::BaseNode* node) {
			if (!node) {
				// if empty space is right clicked
				ImGui::MenuItem("Copy");
				ImGui::MenuItem("Paste");
				ImGui::MenuItem("Search");
				ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
				if (ImGui::MenuItem("Add Node")) { ImGui::OpenPopup("###ADDNODES"); }
				ImGui::PopItemFlag();

				if (ImGui::BeginPopup("###ADDNODES")) {
					bool shouldClose = false;
					// categories
					for (const NodeCategory category : kCATEGORY_ORDER) {
						const auto& nodeDefinitions =
								NodeRegistry::GetNodesInCategory(category);
						if (nodeDefinitions.empty())
							continue;
						if (ImGui::BeginMenu(NodeCategoryToString(category))) {
							for (const auto& nodeDef : nodeDefinitions) {
								if (ImGui::MenuItem(nodeDef.name.c_str())) {
									nodeDef.func(f);
									shouldClose = true;
								}
							}
							ImGui::EndMenu();
						}
					}
					ImGui::Separator();
					// uniques category
					const auto& nodeDefinitions =
							NodeRegistry::GetNodesInCategory(NodeCategory::UNIQUE);
					if (!nodeDefinitions.empty()) {
						for (const auto& nodeDef : nodeDefinitions) {
							if (ImGui::MenuItem(nodeDef.name.c_str())) {
								nodeDef.func(f);
								shouldClose = true;
							}
						}
					}
					ImGui::EndPopup();
					if (shouldClose) { ImGui::CloseCurrentPopup(); }
				}
			} else {
				if (ImGui::MenuItem("Copy")) {}
				if (ImGui::MenuItem("Delete")) { node->destroy(); }
				// if node is right clicked
			}
		});

		
		f.droppedLinkPopUpContent([&f](ImFlow::Pin* dragged) {
			static char searchBuf[128] = {};
			static int selectedIdx = 0;
			static std::string lastFilter;
			ImGui::SetNavCursorVisible(false);

			if (ImGui::IsWindowAppearing()) {
				searchBuf[0] = '\0';
				selectedIdx = 0;
				lastFilter.clear();
				ImGui::SetKeyboardFocusHere();
			}

			ImGui::SetNextItemWidth(250.f);
			ImGui::InputText("##search", searchBuf, sizeof(searchBuf));

			const ConnectionType draggedType = pinType(dragged);
			const bool draggedIsOutput = dragged->getType() == ImFlow::PinType_Output;

			auto tryConnect = [&](const std::shared_ptr<ImFlow::BaseNode>& node) {
				if (!node) return;
				if (draggedIsOutput) {
					for (const auto& pin : node->getIns()) {
						if (CanConnectPins(dragged, pin.get())) { dragged->createLink(pin.get()); break; }
					}
				} else {
					for (const auto& pin : node->getOuts()) {
						if (CanConnectPins(pin.get(), dragged)) { dragged->createLink(pin.get()); break; }
					}
				}
			};

			auto nodeIsCompatible = [&](const auto& def) -> bool {
				if (draggedIsOutput) {
					if (def.acceptedInputs.empty()) return false;
					for (auto t : def.acceptedInputs)
						if (CanConvertConnection(draggedType, t)) return true;
					return false;
				} else {
					if (def.providedOutputs.empty()) return false;
					for (auto t : def.providedOutputs)
						if (CanConvertConnection(t, draggedType)) return true;
					return false;
				}
			};

			// build flat list as "Category > Name"
			struct FlatEntry {
				std::string category;
				std::string name;
				std::function<std::shared_ptr<ImFlow::BaseNode>(ImFlow::ImNodeFlow&)> func;
			};
			std::vector<FlatEntry> entries;

			std::string filter(searchBuf);
			std::ranges::transform(filter, filter.begin(), tolower);

			auto collectCategory = [&](NodeCategory cat) {
				const char* catStr = NodeCategoryToString(cat);
				for (const auto& nodeDef : NodeRegistry::GetNodesInCategory(cat)) {
					if (!nodeIsCompatible(nodeDef)) continue;
					if (!filter.empty()) {
						std::string combined = std::string(catStr) + " > " + nodeDef.name;
						std::ranges::transform(combined, combined.begin(), ::tolower);
						if (combined.find(filter) == std::string::npos) continue;
					}
					entries.push_back({catStr, nodeDef.name, nodeDef.func});
				}
			};

			for (const NodeCategory cat : kCATEGORY_ORDER)
				collectCategory(cat);
			for (const auto& nodeDef : NodeRegistry::GetNodesInCategory(NodeCategory::UNIQUE)) {
				if (!nodeIsCompatible(nodeDef)) continue;
				if (!filter.empty()) {
					std::string combined = std::string("Unique > ") + nodeDef.name;
					std::ranges::transform(combined, combined.begin(), ::tolower);
					if (combined.find(filter) == std::string::npos) continue;
				}
				entries.push_back({"Unique", nodeDef.name, nodeDef.func});
			}

			// reset selection when filter changes
			if (filter != lastFilter) {
				selectedIdx = 0;
				lastFilter = filter;
			}

			const int count = static_cast<int>(entries.size());
			if (selectedIdx >= count) selectedIdx = std::max(0, count - 1);

			// keyboard nav for list
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selectedIdx < count - 1) {
				++selectedIdx;
				ImGui::SetNavCursorVisible(false);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selectedIdx > 0) {
				--selectedIdx;
				ImGui::SetNavCursorVisible(false);
			}

			bool shouldClose = false;
			if (ImGui::IsKeyPressed(ImGuiKey_Enter) && count > 0) {
				tryConnect(entries[selectedIdx].func(f));
				shouldClose = true;
			}

			ImGui::Separator();
			ImGui::BeginChild("##nodelist", ImVec2(250.f, 220.f), ImGuiChildFlags_None,
			                  ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
			ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
			for (int i = 0; i < count; ++i) {
				const bool selected = i == selectedIdx;
				ImGui::PushID(i);
				if (ImGui::Selectable("##row", selected)) {
					tryConnect(entries[i].func(f));
					shouldClose = true;
				}
				const ImVec2 rowMin = ImGui::GetItemRectMin();
				ImGui::SetCursorScreenPos(ImVec2(rowMin.x + ImGui::GetStyle().ItemInnerSpacing.x, rowMin.y));
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.65f), "%s >", entries[i].category.c_str());
				ImGui::SameLine(0.f, 4.f);
				ImGui::TextUnformatted(entries[i].name.c_str());
				ImGui::PopID();
				if (selected) ImGui::SetScrollHereY(0.5f);
			}
			ImGui::PopItemFlag();
			ImGui::EndChild();

			if (shouldClose) ImGui::CloseCurrentPopup();
		});

		// tracking variables
		std::vector<VariableEntry> push_constant_entries;
		size_t currentUserDataSize = sizeof(defaultColor);
		mythril::Shader newFragShader;
		uint64_t recompileCounter = 0;
		bool quit = false;
		bool recompile = false;

		bool isMarqueeSelecting = false;
		ImVec2 marqueeStart = {0, 0};

		// sentinel that won't match the empty graph (cause its 0), so the first frame compiles
		uint64_t lastGraphSignature = ~0ull;
		std::unordered_map<std::string, mythril::Texture> loadedTextures;
		while (!quit) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {
				ImGui_ImplSDL3_ProcessEvent(&e);
				if (e.type == SDL_EVENT_QUIT)
					quit = true;
				if (e.type == SDL_EVENT_KEY_DOWN) {
					if (e.key.key == SDLK_Q && SDL_GetModState() & SDL_KMOD_CTRL)
						quit = true;
					if (e.key.key == SDLK_R && SDL_GetModState() & SDL_KMOD_CTRL) {
						recompile = true;
					}
				}
			}

			// mandatory for resizeability
			const auto& fbSize = GetSDLWindowFramebufferSize(sdlWindow);
			if (ctx->isSwapchainDirty()) {
				ctx->recreateSwapchain({.width = fbSize.width, .height = fbSize.height});
				screenTexture.resize({fbSize.width, fbSize.height, 1});
			}

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL3_NewFrame();
			ImGui::NewFrame();

			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGuiWindowFlags window_flags =
					ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("MainDockSpace", nullptr, window_flags);
			ImGui::PopStyleVar();

			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id,
			                 ImVec2(0.0f, 0.0f),
			                 ImGuiDockNodeFlags_PassthruCentralNode |
			                 ImGuiDockNodeFlags_NoUndocking |
			                 ImGuiDockNodeFlags_NoTabBar);

			static bool first_time = true;
			if (first_time) {
				first_time = false;
				ImGui::DockBuilderRemoveNode(dockspace_id);
				ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

				ImGuiID dock_right = ImGui::DockBuilderSplitNode(
					dockspace_id,
					ImGuiDir_Right,
					0.25f,
					nullptr,
					&dockspace_id);
				ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(
					dock_right,
					ImGuiDir_Down,
					0.5f,
					nullptr,
					&dock_right);

				ImGui::DockBuilderDockWindow("##Node Editor", dockspace_id);
				ImGui::DockBuilderDockWindow("##Preview", dock_right);
				ImGui::DockBuilderDockWindow("##Options", dock_right_bottom);

				ImGui::DockBuilderFinish(dockspace_id);
			}
			ImGui::End();

			ImGuiWindowFlags panel_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("##Node Editor", nullptr, panel_flags);
			ImGui::PopStyleVar();

			ImGui::BeginMenuBar();

			ImGui::PushFont(boldFont, 0.f);
			ImGui::Text("Node Editor");
			ImGui::PopFont();

			ImGui::Separator();
			ImGui::MenuItem("View");
			ImGui::MenuItem("Clean");

			ImGui::EndMenuBar();

#ifdef __APPLE__
			// pan inertia
			{
				ImVec2 s = f.getScroll();
				s.x += gs.panVelocity.x;
				s.y += gs.panVelocity.y;
				f.getGrid().setScroll(s);
				gs.panVelocity.x *= kPanFriction;
				gs.panVelocity.y *= kPanFriction;
				if (std::abs(gs.panVelocity.x) < 0.01f) gs.panVelocity.x = 0.f;
				if (std::abs(gs.panVelocity.y) < 0.01f) gs.panVelocity.y = 0.f;
			}
			// zoom inertia
			if (std::abs(gs.zoomVelocity) > 0.001f) {
				const float oldScale = f.getGrid().scale();
				const float newScale = std::clamp(
					oldScale * (1.f + gs.zoomVelocity),
					f.getGrid().config().zoom_min,
					f.getGrid().config().zoom_max);
				const ImVec2 origin = f.getPos();
				const ImVec2 scroll = f.getScroll();
				f.getGrid().setScale(newScale);
				f.getGrid().setScroll({
					scroll.x + (gs.lastPinchCx - origin.x) / newScale - (gs.lastPinchCx - origin.x) / oldScale,
					scroll.y + (gs.lastPinchCy - origin.y) / newScale - (gs.lastPinchCy - origin.y) / oldScale
				});
				gs.zoomVelocity *= kZoomFriction;
				if (std::abs(gs.zoomVelocity) < 0.001f) gs.zoomVelocity = 0.f;
			}
#endif
			// where imgnodeflow actually updates!!
			f.update();

			// selection box
			{
				const ImVec2 canvasOrigin = f.getPos();
				const float scale = f.getGrid().scale();
				const ImVec2 scroll = f.getScroll();
				const ImVec2 mousePos = ImGui::GetMousePos();
				const bool canvasHovered = f.getGrid().hovered();

				auto screenToCanvas = [&](ImVec2 s) -> ImVec2 {
					return {(s.x - canvasOrigin.x) / scale - scroll.x,
					        (s.y - canvasOrigin.y) / scale - scroll.y};
				};
				bool mouseOverNode = false;
				if (canvasHovered) {
					for (auto& node : f.getNodes() | std::views::values) {
						if (!node || node->toDestroy()) continue;
						const ImVec2 np = node->getPos();
						const ImVec2 ns = node->getSize();
						const float sx = canvasOrigin.x + (np.x + scroll.x) * scale;
						const float sy = canvasOrigin.y + (np.y + scroll.y) * scale;
						if (mousePos.x >= sx && mousePos.x <= sx + ns.x * scale &&
						    mousePos.y >= sy && mousePos.y <= sy + ns.y * scale) {
							mouseOverNode = true;
							break;
						}
					}
				}
				ImGuiContext* outerCtx = ImGui::GetCurrentContext();
				ImGui::SetCurrentContext(f.getGrid().getRawContext());
				const bool innerPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
				ImGui::SetCurrentContext(outerCtx);
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && canvasHovered && !mouseOverNode && !f.isNodeDragged() && !innerPopupOpen) {
					isMarqueeSelecting = true;
					marqueeStart = mousePos;
				}
				if (isMarqueeSelecting) {
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						const ImVec2 rMin = {std::min(marqueeStart.x, mousePos.x), std::min(marqueeStart.y, mousePos.y)};
						const ImVec2 rMax = {std::max(marqueeStart.x, mousePos.x), std::max(marqueeStart.y, mousePos.y)};
						ImDrawList* dl = ImGui::GetForegroundDrawList();
						dl->AddRectFilled(rMin, rMax, IM_COL32(100, 150, 220, 40));
						dl->AddRect(rMin, rMax, IM_COL32(100, 150, 220, 200), 0.f, 0, 1.5f);
					} else {
						isMarqueeSelecting = false;
						const ImVec2 cMin = screenToCanvas({std::min(marqueeStart.x, mousePos.x), std::min(marqueeStart.y, mousePos.y)});
						const ImVec2 cMax = screenToCanvas({std::max(marqueeStart.x, mousePos.x), std::max(marqueeStart.y, mousePos.y)});
						for (auto& node : f.getNodes() | std::views::values) {
							if (!node || node->toDestroy()) continue;
							const ImVec2 np = node->getPos();
							const ImVec2 ns = node->getSize();
							const bool hit = np.x < cMax.x && np.x + ns.x > cMin.x &&
							                 np.y < cMax.y && np.y + ns.y > cMin.y;
							node->selected(hit);
						}
					}
				}
			}
			// hotkeys
			{
				const ImVec2 canvasMin = f.getPos();
				const ImVec2 canvasSize = f.getGrid().size();
				const ImVec2 canvasMax = {canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y};
				const ImVec2 mousePos = ImGui::GetMousePos();
				const bool isMouseOverCanvas = mousePos.x >= canvasMin.x && mousePos.x <= canvasMax.x &&
				                               mousePos.y >= canvasMin.y && mousePos.y <= canvasMax.y;
				static ImVec2 initialMousePos = { 0.0f, 0.0f };
				ImGuiContext* outerCtxHotkey = ImGui::GetCurrentContext();
				ImGui::SetCurrentContext(f.getGrid().getRawContext());
				const bool innerPopupOpenHotkey = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
				ImGui::SetCurrentContext(outerCtxHotkey);
				const bool anyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) || innerPopupOpenHotkey;
				if (isMouseOverCanvas && !anyPopupOpen &&
				    ImGui::Shortcut(ImGuiMod_Shift | ImGuiKey_A, ImGuiInputFlags_RouteGlobal)) {
					ImGui::OpenPopup("NodeSearchMenu");
					initialMousePos = mousePos;
				}
				ImGui::SetNextWindowPos(mousePos, ImGuiCond_Appearing);
				if (ImGui::BeginPopup("NodeSearchMenu", ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus)) {
					static char search_buffer[128] = "";
					static int selected_idx = 0;
					static std::string last_filter;
					ImGui::SetNavCursorVisible(false);

					// set initial focus on new popup
					if (ImGui::IsWindowAppearing()) {
						ImGui::SetKeyboardFocusHere();
						search_buffer[0] = '\0';
						selected_idx = 0;
						last_filter.clear();
					}
					ImGui::TextDisabled("Search Nodes...");
					ImGui::InputText("###search", search_buffer, IM_ARRAYSIZE(search_buffer));
					ImGui::Separator();

					auto spawnAndPlaceNode = [&](const std::function<std::shared_ptr<ImFlow::BaseNode>(ImFlow::ImNodeFlow&)>& spawnFunc) {
						if (auto newNode = spawnFunc(f)) {
							const ImVec2 canvas_origin = f.getPos();
							const float scale = f.getGrid().scale();
							const ImVec2 scroll = f.getScroll();
							const ImVec2 canvas_spawn_pos = {
								(initialMousePos.x - canvas_origin.x) / scale - scroll.x,
								(initialMousePos.y - canvas_origin.y) / scale - scroll.y
							};
							newNode->setPos(canvas_spawn_pos);
						}
					};

					std::string filter(search_buffer);
					std::ranges::transform(filter, filter.begin(), [](unsigned char c) {
						return static_cast<char>(std::tolower(c));
					});

					const auto& allRegisteredNodes = NodeRegistry::GetAllNodes();
					std::vector<int> filteredNodeIndices;
					filteredNodeIndices.reserve(allRegisteredNodes.size());
					for (int i = 0; i < static_cast<int>(allRegisteredNodes.size()); ++i) {
						const auto& nodeDef = allRegisteredNodes[i];
						std::string searchable = nodeDef.name;
						if (nodeDef.category == NodeCategory::UNIQUE)
							searchable += " Uniques";
						else
							searchable += std::string(" ") + NodeCategoryToString(nodeDef.category);
						std::ranges::transform(searchable, searchable.begin(), [](unsigned char c) {
							return static_cast<char>(std::tolower(c));
						});
						if (filter.empty() || searchable.find(filter) != std::string::npos)
							filteredNodeIndices.push_back(i);
					}

					if (filter != last_filter) {
						selected_idx = 0;
						last_filter = filter;
					}

					const int count = static_cast<int>(filteredNodeIndices.size());
					if (selected_idx >= count) selected_idx = std::max(0, count - 1);
					// keyboard nav for list
					if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && selected_idx < count - 1) {
						++selected_idx;
						ImGui::SetNavCursorVisible(false);
					}
					if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && selected_idx > 0) {
						--selected_idx;
						ImGui::SetNavCursorVisible(false);
					}


					bool should_close = false;
					if (ImGui::IsKeyPressed(ImGuiKey_Enter) && count > 0) {
						spawnAndPlaceNode(allRegisteredNodes[filteredNodeIndices[selected_idx]].func);
						should_close = true;
					}
					ImGui::BeginChild("###nodelist", ImVec2(250.f, 250.f), ImGuiChildFlags_None,
					                  ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
					ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
					for (int i = 0; i < count; i++) {
						const bool selected = i == selected_idx;
						const auto& nodeDef = allRegisteredNodes[filteredNodeIndices[i]];
						ImGui::PushID(i);
						if (ImGui::Selectable("##row", selected)) {
							spawnAndPlaceNode(nodeDef.func);
							should_close = true;
						}
						const ImVec2 row_min = ImGui::GetItemRectMin();
						ImGui::SetCursorScreenPos(ImVec2(row_min.x + ImGui::GetStyle().ItemInnerSpacing.x, row_min.y));
						//ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.65f), "")
						ImGui::TextUnformatted(nodeDef.name.c_str());
						ImGui::PopID();
						if (selected) ImGui::SetScrollHereY(0.5f);
					}
					ImGui::PopItemFlag();
					ImGui::EndChild();
					if (should_close) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}
			}

			ImGui::End();

			ImGui::Begin("##Preview", nullptr, panel_flags);
			ImGui::PushFont(boldFont, 0.f);
			ImGui::Text("Preview");
			ImGui::PopFont();
			static glm::vec2 viewportBounds[2];
			static glm::vec2 viewportSize;

			ImVec2 viewportPos = ImGui::GetCursorScreenPos(); // top-left corner of the viewport
			ImVec2 availableSize = ImGui::GetContentRegionAvail();
			viewportBounds[0] = {viewportPos.x, viewportPos.y};
			viewportBounds[1] = {viewportPos.x + availableSize.x, viewportPos.y + availableSize.y};

			ImVec2 size = ImGui::GetContentRegionAvail();
			if (viewportSize != *((glm::vec2*) &size) && size.x > 0.0f &&
			    size.y > 0.0f) {
				viewportSize = {size.x, size.y};
				camera.aspectRatio = size.x / size.y;

				// for resizing of the texture itself
				//				ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(imguiTexID));
				//				ctx->resizeTexture(previewTexture,
				//VkExtent2D{static_cast<uint32_t>(size.x),
				//static_cast<uint32_t>(size.y)}); 				mythril::CommandBuffer& cmd2 =
				//ctx->openCommand(mythril::CommandBuffer::Type::Compute);
				//				cmd2.cmdTransitionLayout(previewTexture,
				//VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); 				ctx->submitCommand(cmd2);
				//				imguiTexID =
				//reinterpret_cast<ImTextureID>(ImGui_ImplVulkan_AddTexture(ctx->getDefaultLinearSampler().getSampler(),
				//ctx->getTexture(previewTexture).getImageView(),
				//VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)); 				graph.compile(*ctx);
			}
			ImGui::Image(previewTexture, ImVec2{viewportSize.x, viewportSize.y});
			ImGui::End();

			ImGui::Begin("##Options", nullptr, panel_flags);
			ImGui::End();

			ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoDocking);
			int ww, wh;
			SDL_GetWindowSize(sdlWindow, &ww, &wh);
			ImGui::Text("Framebuffer: %u, %u", fbSize.width, fbSize.height);
			ImGui::Text("Windowsize: %d, %d", ww, wh);

			const mythril::Dimensions colorTargetDims = screenTexture->getDimensions();
			ImGui::Text("Color Texture Size: %.1u x %.1u",
			            colorTargetDims.width,
			            colorTargetDims.height);

			ImGui::Text("[ImGui] Display Size: %.1f x %.1f",
			            io.DisplaySize.x,
			            io.DisplaySize.y);
			ImGui::Text("[ImGui] Display Framebuffer Scale: %.1f x %.1f",
			            io.DisplayFramebufferScale.x,
			            io.DisplayFramebufferScale.y);
			ImGui::Text("ImGui Framerate: %.2f", io.Framerate);
			ImGui::End();

			// there must be an active output ndoe, if not we just reuse last valid state
			if (OutputNode* activeOutputNode = findActiveOutputNode()) {
				// recompute graph signature every frame
				const uint64_t graphSignature = GraphSignature::Compute(activeOutputNode);
				if (graphSignature != lastGraphSignature) {
					lastGraphSignature = graphSignature;
					recompile = true;
				}
				// only when nodes connected to output change do we recompile
				if (recompile) {
					const auto [fileContent, pushLayout, pcSize] = ShaderCompiler::Compile(activeOutputNode);
					//std::cerr << "[Recompile] output=" << activeOutputNode->getUID() << " sig=" << graphSignature << " entries=" << pushLayout.size() << " pcSize=" << pcSize << "\n";
					//std::cerr << fileContent << std::endl;

					newFragShader = ctx->createShader({
						.source = fileContent,
						.debugName = ("graph_shader_" + std::to_string(recompileCounter)).c_str()
					});
					// keep the last good shader and its matching layout bound if compile fails
					if (!newFragShader.valid()) {
						std::cerr << "[ERROR] Shader creation failed!\n";
					} else {
						if (pcSize > currentUserDataSize) {
							ctx->resizeBuffer(userBuffer.handle(), pcSize);
							currentUserDataSize = pcSize;
						}
						push_constant_entries = pushLayout;
						ctx->switchShader(mainPipeline, newFragShader, mythril::ShaderStages::Fragment);
						++recompileCounter;
					}
					recompile = false;
				}
			}

			// load any textures whose path changed this frame
			for (auto& node_ptr : f.getNodes() | std::views::values) {
				// only look at a ready node
				if (!node_ptr || node_ptr->toDestroy()) continue;
				auto* tex_node = dynamic_cast<TextureNode*>(node_ptr.get());
				// is the node a texture_node and does it have new image data?
				if (!tex_node || !tex_node->isPathDirty()) continue;
				const std::string path = tex_node->getFilePath();
				if (path.empty()) { tex_node->clearPathDirty(); continue; }

				int w, h, channels;
				stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
				if (!data) {
					std::cerr << "[TextureNode] Failed to load: " << path << "\n";
					tex_node->clearPathDirty();
					continue;
				}
				mythril::Texture tex = ctx->createTexture({
					.dimension = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1},
					.format    = VK_FORMAT_R8G8B8A8_UNORM,
					.usage     = mythril::TextureUsageBits_Sampled,
					.storage   = mythril::StorageType::Device,
					.debugName = path.c_str(),
				});
				ctx->upload(tex.handle(), data, mythril::TexRange{
					.dimensions = {
						static_cast<uint32_t>(w),
						static_cast<uint32_t>(h),
						1}
				});
				stbi_image_free(data);
				tex_node->applyHandle(tex.handle());
				tex_node->setTextureIndex(tex.index());
				tex_node->clearPathDirty();
				loadedTextures[path] = std::move(tex);
			}

			// build the user_data to send to shader
			std::vector<uint8_t> user_data(currentUserDataSize, 0);
			for (const auto& entry : push_constant_entries) {
				if (!entry.src) continue;
				if (entry.offset + entry.size > user_data.size()) {
					std::cerr << "Push constant write out of bounds for " << entry.varName
					          << ": offset=" << entry.offset << " size=" << entry.size
					          << " buffer=" << user_data.size() << "\n";
					assert(false && "Push constant value write exceeds user buffer size");
				}
				std::memcpy(user_data.data() + entry.offset, entry.src, entry.size);
			}

			const auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float>(currentTime - startTime).count();
			const GPU::Camera camera_data = {
				.proj = calculateProjectionMatrix(camera),
				.view = calculateViewMatrix(camera),
				.position = camera.position
			};
			const GPU::FrameData frame_data = {
				.camera = camera_data,
				.time = time,
				.resolution = {1920, 1080},
			};
			mythril::CommandBuffer& cmd = ctx->acquireCommand(mythril::CommandBuffer::Type::Graphics);
			cmd.cmdUpdateBuffer(perFrameBuffer, frame_data);
			// just upload every frame doesnt hurt
			if (!user_data.empty()) {
				cmd.cmdUpdateBuffer(userBuffer, user_data);
			}
			graph.execute(cmd);
			ctx->submitCommand(cmd);
		}

#ifdef __APPLE__
		Andesite::UnregisterMacOSGestures();
#endif
	}
} // namespace Andesite

int main() {
	Andesite::n_main();
	return 0;
}
