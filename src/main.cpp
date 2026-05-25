#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <mythril/CTX.h>
#include <mythril/CTXBuilder.h>
#include <mythril/RenderGraphBuilder.h>

#include <chrono>
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

#include "Compiler.h"
#include "NodeBasics.h"
#include "ShaderNodes.h"
#include "ConvertNodes.h"
#include "InputNodes.h"

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
			.initialData = (float*)&defaultColor,
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
		//		f.getStyle().grid_size = 100.f;
		//		f.getStyle().grid_subdivisions = 2.f;
		f.addNode<ScalarNode>({});
		f.addNode<ScalarNode>({50, 100});

		f.addNode<MathNode>({250, 100});
		auto rgbNode = f.addNode<RGBNode>({200, 300});
		auto pbrNode = f.addNode<PBRNode>({500, 300});
		auto output1 = f.addNode<OutputNode>({800, 350});
		rgbNode->getOuts()[0]->createLink(pbrNode->getIns()[0].get());
		pbrNode->getOuts()[0]->createLink(output1->getIns()[0].get());

		auto findActiveOutputNode = [&f, &output1]() -> OutputNode* {
			OutputNode* firstOutput = nullptr;
			if (output1 && !output1->toDestroy()) {
				firstOutput = output1.get();
			}

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
		NodeRegistry::RegisterNode<SeperateXYZ>("Seperate Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<CombineXYZ>("Combine Node", NodeCategory::CONVERT);
		NodeRegistry::RegisterNode<PBRNode>("PBR Node", NodeCategory::SHADER);
		NodeRegistry::RegisterNode<OutputNode>("Output Node", NodeCategory::SINGLES);
		NodeRegistry::RegisterNode<MixNode>("Mix Node", NodeCategory::CONVERT);

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
					// single category
					const auto& nodeDefinitions =
							NodeRegistry::GetNodesInCategory(NodeCategory::SINGLES);
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

		// tracking variables
		std::vector<PushConstantEntry> push_constant_entries;
		size_t currentPCSize = sizeof(defaultColor);
		size_t currentUserDataSize = 0;
		mythril::Shader newFragShader;
		uint64_t recompileCounter = 0;
		bool quit = false;
		bool recompile = false;

		// sentinel that won't match the empty graph (0), so the first frame compiles
		uint64_t lastGraphSignature = ~0ull;
		std::unordered_map<std::string, mythril::Texture> loadedTextures;
		while (!quit) {
			SDL_Event e;
			while (SDL_PollEvent(&e)) {
				ImGui_ImplSDL3_ProcessEvent(&e);
				if (e.type == SDL_EVENT_QUIT)
					quit = true;
				if (e.type == SDL_EVENT_KEY_DOWN) {
					if (e.key.key == SDLK_Q)
						quit = true;
					if (e.key.key == SDLK_R) {
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

			f.update();
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
					if (pcSize > currentPCSize) {
						ctx->resizeBuffer(userBuffer.handle(), pcSize);
						currentPCSize = pcSize;
					}

					push_constant_entries = pushLayout;
					currentUserDataSize = pcSize;

					//std::cerr << "[Recompile] output=" << activeOutputNode->getUID() << " sig=" << graphSignature << " entries=" << pushLayout.size() << " pcSize=" << pcSize << "\n";
					std::cerr << fileContent << std::endl;

					newFragShader = ctx->createShader({
						.source = fileContent,
						.debugName = ("graph_shader_" + std::to_string(recompileCounter)).c_str()
					});
					if (!newFragShader.valid()) {
						std::cerr << "[ERROR] Shader creation failed!\n";
					}
					ctx->switchShader(mainPipeline, newFragShader, mythril::ShaderStages::Fragment);
					recompile = false;
					// simple, optional tracking
					++recompileCounter;
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
			auto copyEntry = [&user_data](const PushConstantEntry& entry, const void* srcPtr, size_t srcSize) {
				if (srcSize != entry.size) {
					std::cerr << "Push constant size mismatch for " << entry.varName
						<< "of type " << PushConstantTypeToShaderString(entry.type)
						<< ": value=" << srcSize << " layout=" << entry.size << "\n";
					assert(false && "Push constant value size differs from layout size");
				}
				if (entry.offset + srcSize > user_data.size()) {
					std::cerr << "Push constant write out of bounds for " << entry.varName
					          << ": offset=" << entry.offset << " size=" << srcSize
					          << " buffer=" << user_data.size() << "\n";
					assert(false && "Push constant value write exceeds user buffer size");
				}
				std::memcpy(user_data.data() + entry.offset, srcPtr, srcSize);
			};
			auto copyEntryWrapper = [&copyEntry](const PushConstantEntry& entry, const auto& value) {
				copyEntry(entry, &value, sizeof(value));
			};

			// todo: all input nodes MUST be here
			for (const auto& entry : push_constant_entries) {
				if (!entry.pNode) continue;
				if (auto* value_node = dynamic_cast<ScalarNode*>(entry.pNode)) {
					const void* v = value_node->getRawDataPtr();
					const size_t s = value_node->getDataSize();
					copyEntry(entry, v, s);
				} else if (auto* rgb_node = dynamic_cast<RGBNode*>(entry.pNode)) {
					const glm::vec3 c = rgb_node->getValue();
					copyEntryWrapper(entry, c);
				} else if (auto* texture_node = dynamic_cast<TextureNode*>(entry.pNode)) {
					const uint64_t index = texture_node->getValue();
					copyEntryWrapper(entry, index);
				} else {
					std::cerr << "Unhandled input node during user buffer packing!\n";
				}
			}

			mythril::CommandBuffer& cmd = ctx->acquireCommand(mythril::CommandBuffer::Type::Graphics);
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
			cmd.cmdUpdateBuffer(perFrameBuffer, frame_data);
			// just upload every frame doesnt hurt
			if (!user_data.empty()) {
				cmd.cmdUpdateBuffer(userBuffer, user_data);
			}
			graph.execute(cmd);
			ctx->submitCommand(cmd);
		}
	}
} // namespace Andesite

int main() {
	Andesite::n_main();
	return 0;
}
