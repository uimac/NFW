#include "Simple.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include "ShaderStorage.h"
#include "Shader.h"
#include "TextureStorage.h"
#include "Texture.h"

namespace nfw
{
	constexpr nri::Color32f COLOR_0 = { 1.0f, 1.0f, 0.0f, 1.0f };
	constexpr nri::Color32f COLOR_1 = { 0.46f, 0.72f, 0.0f, 1.0f };

	struct ConstantBufferLayout
	{
		float color[3];
		float scale;
	};

	struct Vertex
	{
		float position[2];
		float uv[2];
	};

	static const Vertex g_vertexData[] =
	{
		{-0.5f, -0.5f, 0.0f, 1.0f},
		{-0.5f,  0.5f, 0.0f, 0.0f},
		{ 0.5f, -0.5f, 1.0f, 1.0f},
		{-0.5f,  0.5f, 0.0f, 0.0f},
		{ 0.5f,  0.5f, 1.0f, 0.0f},
		{ 0.5f, -0.5f, 1.0f, 1.0f},
	};

	static const uint16_t g_indexData[] = { 0, 1, 2, 3, 4, 5 };

	struct Frame
	{
		nri::CommandAllocator* commandAllocator;
		nri::CommandBuffer* commandBuffer;
		nri::Descriptor* constantBufferView;
		nri::DescriptorSet* constantBufferDescriptorSet;
		uint64_t constantBufferViewOffset;
	};

	struct BackBuffer
	{
		nri::Descriptor* colorAttachment;
		nri::Texture* texture;
	};

	constexpr uint32_t BUFFERED_FRAME_MAX_NUM = 2;
	constexpr uint32_t SWAP_CHAIN_TEXTURE_NUM = BUFFERED_FRAME_MAX_NUM;

	size_t Align(size_t location, size_t align)
	{
		if ((0 == align) || (align & (align - 1)))
		{
			throw std::exception("non-pow2 alignment");
		}
		return ((location + (align - 1)) & ~(align - 1));
	}

	template <typename T, typename U>
	constexpr uint32_t GetOffsetOf(U T::* member) {
		return (uint32_t)((char*)&((T*)nullptr->*member) - (char*)nullptr);
	}

	class Simple::Impl
	{
	public:
		Impl::Impl(void* hwnd, glm::uvec2 resolution)
		{
			m_window.windows.hwnd = hwnd;
			m_resolution = resolution;
		}

		Impl::~Impl()
		{
			NRI.WaitForIdle(*m_commandQueue);

			for (Frame& frame : m_frames)
			{
				NRI.DestroyCommandBuffer(*frame.commandBuffer);
				NRI.DestroyCommandAllocator(*frame.commandAllocator);
				NRI.DestroyDescriptor(*frame.constantBufferView);
			}

			for (BackBuffer& backBuffer : m_backBuffers)
			{
				NRI.DestroyDescriptor(*backBuffer.colorAttachment);
			}

			NRI.DestroyPipeline(*m_pipeline);
			NRI.DestroyPipelineLayout(*m_pipelineLayout);
			m_textureStorage = nullptr;
			NRI.DestroyDescriptor(*m_sampler);
			NRI.DestroyBuffer(*m_constantBuffer);
			NRI.DestroyBuffer(*m_geometryBuffer);
			NRI.DestroyDescriptorPool(*m_descriptorPool);
			NRI.DestroyFence(*m_frameFence);
			NRI.DestroySwapChain(*m_swapChain);

			nri::nriDestroyDevice(*m_device);
		}

		bool Init()
		{
			nri::GraphicsAPI graphicsAPI = nri::GraphicsAPI::D3D12;
			nri::AdapterDesc adapterDesc = {};
			uint32_t adapterDescNum = 1;
			NRI_ABORT_ON_FAILURE(nri::nriEnumerateAdapters(&adapterDesc, adapterDescNum));

			// Device
			nri::DeviceCreationDesc deviceCreationDesc = {};
			deviceCreationDesc.graphicsAPI = graphicsAPI;
			deviceCreationDesc.enableGraphicsAPIValidation = false;
			deviceCreationDesc.enableNRIValidation = false;
			deviceCreationDesc.enableD3D11CommandBufferEmulation = false;
			deviceCreationDesc.spirvBindingOffsets = { 100, 200, 300, 400 };
			deviceCreationDesc.adapterDesc = &adapterDesc;
			deviceCreationDesc.allocationCallbacks = {};
			NRI_ABORT_ON_FAILURE(nri::nriCreateDevice(deviceCreationDesc, m_device));

			// NRI
			NRI_ABORT_ON_FAILURE(nri::nriGetInterface(*m_device, NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)&NRI));
			NRI_ABORT_ON_FAILURE(nri::nriGetInterface(*m_device, NRI_INTERFACE(nri::SwapChainInterface), (nri::SwapChainInterface*)&NRI));
			NRI_ABORT_ON_FAILURE(nri::nriGetInterface(*m_device, NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)&NRI));

			// Command queue
			NRI_ABORT_ON_FAILURE(NRI.GetCommandQueue(*m_device, nri::CommandQueueType::GRAPHICS, m_commandQueue));

			// Fences
			NRI_ABORT_ON_FAILURE(NRI.CreateFence(*m_device, 0, m_frameFence));

			// Swap chain
			nri::Format swapChainFormat;
			{
				nri::SwapChainDesc swapChainDesc = {};
				swapChainDesc.window = m_window;
				swapChainDesc.commandQueue = m_commandQueue;
				swapChainDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
				swapChainDesc.verticalSyncInterval = 0;
				swapChainDesc.width = m_resolution.x;
				swapChainDesc.height = m_resolution.y;
				swapChainDesc.textureNum = SWAP_CHAIN_TEXTURE_NUM;
				NRI_ABORT_ON_FAILURE(NRI.CreateSwapChain(*m_device, swapChainDesc, m_swapChain));

				uint32_t swapChainTextureNum;
				nri::Texture* const* swapChainTextures = NRI.GetSwapChainTextures(*m_swapChain, swapChainTextureNum);
				swapChainFormat = NRI.GetTextureDesc(*swapChainTextures[0]).format;

				for (uint32_t i = 0; i < swapChainTextureNum; i++)
				{
					nri::Texture2DViewDesc textureViewDesc = { swapChainTextures[i], nri::Texture2DViewType::COLOR_ATTACHMENT, swapChainFormat };

					nri::Descriptor* colorAttachment;
					NRI_ABORT_ON_FAILURE(NRI.CreateTexture2DView(textureViewDesc, colorAttachment));

					const BackBuffer backBuffer = { colorAttachment, swapChainTextures[i] };
					m_backBuffers.push_back(backBuffer);
				}
			}

			// Buffered resources
			for (Frame& frame : m_frames)
			{
				NRI_ABORT_ON_FAILURE(NRI.CreateCommandAllocator(*m_commandQueue, frame.commandAllocator));
				NRI_ABORT_ON_FAILURE(NRI.CreateCommandBuffer(*frame.commandAllocator, frame.commandBuffer));
			}

			InitPipeline(swapChainFormat);
			InitDescriptorPool();
			InitResources();

			return true;
		}

		void InitPipeline(nri::Format swapChainFormat)
		{
			const nri::DeviceDesc& deviceDesc = NRI.GetDeviceDesc(*m_device);
			ShaderStorage shaderStorage;

			// PipelineLayout
			{
				nri::DescriptorRangeDesc descriptorRangeConstant[1];
				descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER, nri::StageBits::ALL };

				nri::DescriptorRangeDesc descriptorRangeTexture[2];
				descriptorRangeTexture[0] = { 0, 1, nri::DescriptorType::TEXTURE, nri::StageBits::FRAGMENT_SHADER };
				descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER, nri::StageBits::FRAGMENT_SHADER };

				nri::DescriptorSetDesc descriptorSetDescs[] =
				{
					{0, descriptorRangeConstant, std::size(descriptorRangeConstant)},
					{1, descriptorRangeTexture, std::size(descriptorRangeTexture)},
				};

				nri::RootConstantDesc pushConstant = { 1, sizeof(float), nri::StageBits::FRAGMENT_SHADER };

				nri::PipelineLayoutDesc pipelineLayoutDesc = {};
				pipelineLayoutDesc.descriptorSetNum = std::size(descriptorSetDescs);
				pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
				pipelineLayoutDesc.rootConstantNum = 1;
				pipelineLayoutDesc.rootConstants = &pushConstant;
				pipelineLayoutDesc.shaderStages = nri::StageBits::VERTEX_SHADER | nri::StageBits::FRAGMENT_SHADER;

				NRI_ABORT_ON_FAILURE(NRI.CreatePipelineLayout(*m_device, pipelineLayoutDesc, m_pipelineLayout));
			}

			{
				nri::VertexStreamDesc vertexStreamDesc = {};
				vertexStreamDesc.bindingSlot = 0;
				vertexStreamDesc.stride = sizeof(Vertex);

				nri::VertexAttributeDesc vertexAttributeDesc[2] = {};
				{
					vertexAttributeDesc[0].format = nri::Format::RG32_SFLOAT;
					vertexAttributeDesc[0].streamIndex = 0;
					vertexAttributeDesc[0].offset = GetOffsetOf(&Vertex::position);
					vertexAttributeDesc[0].d3d = { "POSITION", 0 };
					vertexAttributeDesc[0].vk.location = { 0 };

					vertexAttributeDesc[1].format = nri::Format::RG32_SFLOAT;
					vertexAttributeDesc[1].streamIndex = 0;
					vertexAttributeDesc[1].offset = GetOffsetOf(&Vertex::uv);
					vertexAttributeDesc[1].d3d = { "TEXCOORD", 0 };
					vertexAttributeDesc[1].vk.location = { 1 };
				}

				nri::VertexInputDesc vertexInputDesc = {};
				vertexInputDesc.attributes = vertexAttributeDesc;
				vertexInputDesc.attributeNum = (uint8_t)std::size(vertexAttributeDesc);
				vertexInputDesc.streams = &vertexStreamDesc;
				vertexInputDesc.streamNum = 1;

				nri::InputAssemblyDesc inputAssemblyDesc = {};
				inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;

				nri::RasterizationDesc rasterizationDesc = {};
				rasterizationDesc.viewportNum = 1;
				rasterizationDesc.fillMode = nri::FillMode::SOLID;
				rasterizationDesc.cullMode = nri::CullMode::NONE;

				nri::ColorAttachmentDesc colorAttachmentDesc = {};
				colorAttachmentDesc.format = swapChainFormat;
				colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
				colorAttachmentDesc.blendEnabled = true;
				colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA, nri::BlendFactor::ONE_MINUS_SRC_ALPHA, nri::BlendFunc::ADD };

				nri::OutputMergerDesc outputMergerDesc = {};
				outputMergerDesc.colorNum = 1;
				outputMergerDesc.colors = &colorAttachmentDesc;

				ShaderConstPtr vertexShader = shaderStorage.LoadShaderFromFile(deviceDesc.graphicsAPI, "Simple.vs");
				ShaderConstPtr pixelShader = shaderStorage.LoadShaderFromFile(deviceDesc.graphicsAPI, "Simple.fs");

				nri::ShaderDesc shaderStages[] =
				{
					vertexShader->GetShaderDesc(),
					pixelShader->GetShaderDesc(),
				};

				nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
				graphicsPipelineDesc.pipelineLayout = m_pipelineLayout;
				graphicsPipelineDesc.vertexInput = &vertexInputDesc;
				graphicsPipelineDesc.inputAssembly = inputAssemblyDesc;
				graphicsPipelineDesc.rasterization = rasterizationDesc;
				graphicsPipelineDesc.outputMerger = outputMergerDesc;
				graphicsPipelineDesc.shaders = shaderStages;
				graphicsPipelineDesc.shaderNum = std::size(shaderStages);

				NRI_ABORT_ON_FAILURE(NRI.CreateGraphicsPipeline(*m_device, graphicsPipelineDesc, m_pipeline));
			}
		}

		void InitDescriptorPool()
		{
			nri::DescriptorPoolDesc descriptorPoolDesc = {};
			descriptorPoolDesc.descriptorSetMaxNum = BUFFERED_FRAME_MAX_NUM + 1;
			descriptorPoolDesc.constantBufferMaxNum = BUFFERED_FRAME_MAX_NUM;
			descriptorPoolDesc.textureMaxNum = 1;
			descriptorPoolDesc.samplerMaxNum = 1;

			NRI_ABORT_ON_FAILURE(NRI.CreateDescriptorPool(*m_device, descriptorPoolDesc, m_descriptorPool));
		}

		bool InitResources()
		{
			const nri::DeviceDesc& deviceDesc = NRI.GetDeviceDesc(*m_device);
			// Load texture
			m_textureStorage = std::make_shared<TextureStorage>(NRI);
			nfw::TexturePtr texture = m_textureStorage->LoadFromFile("../../resource/texture/uimac.jpeg");
			if (!texture)
			{
				return false;
			}

			// Resources
			const uint32_t constantBufferSize = Align((uint32_t)sizeof(ConstantBufferLayout), deviceDesc.constantBufferOffsetAlignment);
			const uint64_t indexDataSize = sizeof(g_indexData);
			const uint64_t indexDataAlignedSize = Align(indexDataSize, 16);
			const uint64_t vertexDataSize = sizeof(g_vertexData);
			{
				nri::TextureDesc textureDesc = texture->GetTextureDesc();
				// Texture
				texture->CreateTexture(NRI, *m_device);

				// Constant buffer
				{
					nri::BufferDesc bufferDesc = {};
					bufferDesc.size = constantBufferSize * BUFFERED_FRAME_MAX_NUM;
					bufferDesc.usageMask = nri::BufferUsageBits::CONSTANT_BUFFER;
					NRI_ABORT_ON_FAILURE(NRI.CreateBuffer(*m_device, bufferDesc, m_constantBuffer));
				}

				// Geometry buffer
				{
					nri::BufferDesc bufferDesc = {};
					bufferDesc.size = indexDataAlignedSize + vertexDataSize;
					bufferDesc.usageMask = nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER;
					NRI_ABORT_ON_FAILURE(NRI.CreateBuffer(*m_device, bufferDesc, m_geometryBuffer));
				}
				m_geometryOffset = indexDataAlignedSize;
			}

			nri::ResourceGroupDesc resourceGroupDesc = {};
			resourceGroupDesc.memoryLocation = nri::MemoryLocation::HOST_UPLOAD;
			resourceGroupDesc.bufferNum = 1;
			resourceGroupDesc.buffers = &m_constantBuffer;

			m_memoryAllocations.resize(1, nullptr);
			NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_device, resourceGroupDesc, m_memoryAllocations.data()));

			resourceGroupDesc.memoryLocation = nri::MemoryLocation::DEVICE;
			resourceGroupDesc.bufferNum = 1;
			resourceGroupDesc.buffers = &m_geometryBuffer;
			resourceGroupDesc.textureNum = 1;
			nri::Texture* texturePtr = texture->GetTexture();
			resourceGroupDesc.textures = &texturePtr;

			m_memoryAllocations.resize(1 + NRI.CalculateAllocationNumber(*m_device, resourceGroupDesc), nullptr);
			NRI_ABORT_ON_FAILURE(NRI.AllocateAndBindMemory(*m_device, resourceGroupDesc, m_memoryAllocations.data() + 1));

			// Descriptors
			{
				// Texture
				nri::Texture2DViewDesc texture2DViewDesc = texture->GetTexture2DViewDesc();
				NRI_ABORT_ON_FAILURE(m_textureStorage->CreateTexture2DView());

				// Sampler
				nri::SamplerDesc samplerDesc = {};
				samplerDesc.anisotropy = 4;
				samplerDesc.addressModes = { nri::AddressMode::MIRRORED_REPEAT, nri::AddressMode::MIRRORED_REPEAT };
				samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
				samplerDesc.anisotropy = 4;
				samplerDesc.mipMax = 16.0f;
				NRI_ABORT_ON_FAILURE(NRI.CreateSampler(*m_device, samplerDesc, m_sampler));

				// Constant buffer
				for (uint32_t i = 0; i < BUFFERED_FRAME_MAX_NUM; i++)
				{
					nri::BufferViewDesc bufferViewDesc = {};
					bufferViewDesc.buffer = m_constantBuffer;
					bufferViewDesc.viewType = nri::BufferViewType::CONSTANT;
					bufferViewDesc.offset = i * constantBufferSize;
					bufferViewDesc.size = constantBufferSize;
					NRI_ABORT_ON_FAILURE(NRI.CreateBufferView(bufferViewDesc, m_frames[i].constantBufferView));

					m_frames[i].constantBufferViewOffset = bufferViewDesc.offset;
				}
			}

			// Descriptor sets
			{
				// Texture
				NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(*m_descriptorPool, *m_pipelineLayout, 1,
					&m_textureDescriptorSet, 1, 0));

				nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
				descriptorRangeUpdateDescs[0].descriptorNum = 1;
				nri::Descriptor* descriptor = m_textureStorage->GetTextureShaderDescriptor();
				descriptorRangeUpdateDescs[0].descriptors = &descriptor;

				descriptorRangeUpdateDescs[1].descriptorNum = 1;
				descriptorRangeUpdateDescs[1].descriptors = &m_sampler;
				NRI.UpdateDescriptorRanges(*m_textureDescriptorSet, 0, std::size(descriptorRangeUpdateDescs), descriptorRangeUpdateDescs);

				// Constant buffer
				for (Frame& frame : m_frames)
				{
					NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(*m_descriptorPool, *m_pipelineLayout, 0, &frame.constantBufferDescriptorSet, 1, 0));

					nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDesc = { &frame.constantBufferView, 1 };
					NRI.UpdateDescriptorRanges(*frame.constantBufferDescriptorSet, 0, 1, &descriptorRangeUpdateDesc);
				}
			}

			// Upload data
			{
				std::vector<uint8_t> geometryBufferData(indexDataAlignedSize + vertexDataSize);
				memcpy(&geometryBufferData[0], g_indexData, indexDataSize);
				memcpy(&geometryBufferData[indexDataAlignedSize], g_vertexData, vertexDataSize);

				nri::TextureUploadDesc textureData = texture->GetTextureUploadDesc();

				nri::BufferUploadDesc bufferData = {};
				bufferData.buffer = m_geometryBuffer;
				bufferData.data = &geometryBufferData[0];
				bufferData.dataSize = geometryBufferData.size();
				bufferData.after = { nri::AccessBits::INDEX_BUFFER | nri::AccessBits::VERTEX_BUFFER };

				NRI_ABORT_ON_FAILURE(NRI.UploadData(*m_commandQueue, &textureData, 1, &bufferData, 1));
			}
			return true;
		}

		void SetResolution(glm::uvec2 resolution)
		{
			m_resolution = resolution;
		}

		void Prepare(uint32_t frameIndex)
		{
		}

		void Render(uint32_t frameIndex)
		{
			const nri::Dim_t windowWidth = static_cast<int16_t>(m_resolution.x);
			const nri::Dim_t windowHeight = static_cast<int16_t>(m_resolution.y);
			const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;
			const Frame& frame = m_frames[bufferedFrameIndex];

			const uint32_t currentTextureIndex = NRI.AcquireNextSwapChainTexture(*m_swapChain);
			BackBuffer& currentBackBuffer = m_backBuffers[currentTextureIndex];

			if (frameIndex >= BUFFERED_FRAME_MAX_NUM)
			{
				NRI.Wait(*m_frameFence, 1 + frameIndex - BUFFERED_FRAME_MAX_NUM);
				NRI.ResetCommandAllocator(*frame.commandAllocator);
			}

			ConstantBufferLayout* constants = (ConstantBufferLayout*)NRI.MapBuffer(*m_constantBuffer, frame.constantBufferViewOffset, sizeof(ConstantBufferLayout));
			if (constants)
			{
				constants->color[0] = 1.0f;
				constants->color[1] = 1.0f;
				constants->color[2] = 1.0f;
				constants->scale = m_scale;

				NRI.UnmapBuffer(*m_constantBuffer);
			}

			nri::TextureBarrierDesc  textureBarrierDesc = {};
			textureBarrierDesc.texture = currentBackBuffer.texture;
			textureBarrierDesc.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT };
			textureBarrierDesc.layerNum = 1;
			textureBarrierDesc.mipNum = 1;

			nri::CommandBuffer* commandBuffer = frame.commandBuffer;
			NRI.BeginCommandBuffer(*commandBuffer, m_descriptorPool);
			{
				nri::BarrierGroupDesc barrierGroupDesc = {};
				barrierGroupDesc.textureNum = 1;
				barrierGroupDesc.textures = &textureBarrierDesc;
				NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);

				nri::AttachmentsDesc attachmentsDesc = {};
				attachmentsDesc.colorNum = 1;
				attachmentsDesc.colors = &currentBackBuffer.colorAttachment;

				NRI.CmdBeginRendering(*commandBuffer, attachmentsDesc);
				{
					{
						//helper::Annotation annotation(NRI, *commandBuffer, "Clear");

						nri::Dim_t halfWidth = windowWidth / 2;
						nri::Dim_t halfHeight = windowHeight / 2;

						nri::ClearDesc clearDesc = {};
						clearDesc.colorAttachmentIndex = 0;
						clearDesc.planes = nri::PlaneBits::COLOR;
						clearDesc.value.color.f = COLOR_0;
						NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, nullptr, 0);

						clearDesc.value.color.f = COLOR_1;
						nri::Rect rects[2];
						rects[0] = { 0, 0, halfWidth, halfHeight };
						rects[1] = { (int16_t)halfWidth, (int16_t)halfHeight, halfWidth, halfHeight };
						NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, rects, std::size(rects));
					}

					{
						//helper::Annotation annotation(NRI, *commandBuffer, "Triangle");

						const nri::Viewport viewport = { 0.0f, 0.0f, (float)windowWidth, (float)windowHeight, 0.0f, 1.0f };
						NRI.CmdSetViewports(*commandBuffer, &viewport, 1);

						NRI.CmdSetPipelineLayout(*commandBuffer, *m_pipelineLayout);
						NRI.CmdSetPipeline(*commandBuffer, *m_pipeline);
						NRI.CmdSetRootConstants(*commandBuffer, 0, &m_transparency, 4);
						NRI.CmdSetIndexBuffer(*commandBuffer, *m_geometryBuffer, 0, nri::IndexType::UINT16);
						NRI.CmdSetVertexBuffers(*commandBuffer, 0, 1, &m_geometryBuffer, &m_geometryOffset);

						NRI.CmdSetDescriptorSet(*commandBuffer, 0, *frame.constantBufferDescriptorSet, nullptr);
						NRI.CmdSetDescriptorSet(*commandBuffer, 1, *m_textureDescriptorSet, nullptr);

						nri::Rect scissor = { 0, 0, (nri::Dim_t)(windowWidth), (nri::Dim_t)(windowHeight) };
						NRI.CmdSetScissors(*commandBuffer, &scissor, 1);
						NRI.CmdDrawIndexed(*commandBuffer, { 6, 1, 0, 0, 0 });
					}

					//RenderUserInterface(*commandBuffer);
				}
				NRI.CmdEndRendering(*commandBuffer);

				textureBarrierDesc.before = textureBarrierDesc.after;
				textureBarrierDesc.after = { nri::AccessBits::UNKNOWN, nri::Layout::PRESENT };

				NRI.CmdBarrier(*commandBuffer, barrierGroupDesc);
			}
			NRI.EndCommandBuffer(*commandBuffer);

			{ // Submit
				nri::QueueSubmitDesc queueSubmitDesc = {};
				queueSubmitDesc.commandBuffers = &frame.commandBuffer;
				queueSubmitDesc.commandBufferNum = 1;
				NRI.QueueSubmit(*m_commandQueue, queueSubmitDesc);
			}

			NRI.QueuePresent(*m_swapChain);

			{ // Signaling after "Present" improves D3D11 performance a bit
				nri::FenceSubmitDesc signalFence = {};
				signalFence.fence = m_frameFence;
				signalFence.value = 1 + frameIndex;

				nri::QueueSubmitDesc queueSubmitDesc = {};
				queueSubmitDesc.signalFences = &signalFence;
				queueSubmitDesc.signalFenceNum = 1;

				NRI.QueueSubmit(*m_commandQueue, queueSubmitDesc);
			}
		}

		void Render2(uint32_t frameIndex)
		{
			const uint32_t windowWidth = m_resolution.x;
			const uint32_t windowHeight = m_resolution.y;
			const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;
			const Frame& frame = m_frames[bufferedFrameIndex];

			const uint32_t backBufferIndex = NRI.AcquireNextSwapChainTexture(*m_swapChain);
			const BackBuffer& backBuffer = m_backBuffers[backBufferIndex];

			if (frameIndex >= BUFFERED_FRAME_MAX_NUM)
			{
				NRI.Wait(*m_frameFence, 1 + frameIndex - BUFFERED_FRAME_MAX_NUM);
				NRI.ResetCommandAllocator(*frame.commandAllocator);
			}

			/*
			nri::CommandBuffer& commandBuffer = *frame.commandBuffer;
			NRI.BeginCommandBuffer(commandBuffer, nullptr, 0);
			{
				nri::TextureBarrierDesc textureBarrierDesc = {};
				textureBarrierDesc.texture = backBuffer.texture;
				textureBarrierDesc.prevAccess = nri::AccessBits::UNKNOWN;
				textureBarrierDesc.nextAccess = nri::AccessBits::COLOR_ATTACHMENT;
				textureBarrierDesc.prevLayout = nri::TextureLayout::UNKNOWN;
				textureBarrierDesc.nextLayout = nri::TextureLayout::COLOR_ATTACHMENT;
				textureBarrierDesc.arraySize = 1;
				textureBarrierDesc.mipNum = 1;

				nri::TransitionBarrierDesc transitionBarriers = {};
				transitionBarriers.textureNum = 1;
				transitionBarriers.textures = &textureBarrierDesc;
				NRI.CmdPipelineBarrier(commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);

				NRI.CmdBeginRenderPass(commandBuffer, *backBuffer.frameBuffer, nri::RenderPassBeginFlag::NONE);
				{
					nri::ClearDesc clearDesc = {};
					clearDesc.colorAttachmentIndex = 0;

					clearDesc.value.color32f = { 0.0f, 90.0f / 255.0f, 187.0f / 255.0f, 1.0f };
					nri::Rect rect1 = { 0, 0, windowWidth, windowHeight / 2 };
					NRI.CmdClearAttachments(commandBuffer, &clearDesc, 1, &rect1, 1);

					clearDesc.value.color32f = { 1.0f, 213.0f / 255.0f, 0.0f, 1.0f };
					nri::Rect rect2 = { 0, (int32_t)windowHeight / 2, windowWidth, windowHeight / 2 };
					NRI.CmdClearAttachments(commandBuffer, &clearDesc, 1, &rect2, 1);
				}
				NRI.CmdEndRenderPass(commandBuffer);

				textureBarrierDesc.prevAccess = textureBarrierDesc.nextAccess;
				textureBarrierDesc.nextAccess = nri::AccessBits::UNKNOWN;
				textureBarrierDesc.prevLayout = textureBarrierDesc.nextLayout;
				textureBarrierDesc.nextLayout = nri::TextureLayout::PRESENT;

				NRI.CmdPipelineBarrier(commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);
			}
			NRI.EndCommandBuffer(commandBuffer);

			const nri::CommandBuffer* commandBufferArray[] = { &commandBuffer };

			nri::QueueSubmitDesc queueSubmitDesc = {};
			queueSubmitDesc.commandBuffers = &frame.commandBuffer;
			queueSubmitDesc.commandBufferNum = 1;
			NRI.QueueSubmit(*m_commandQueue, queueSubmitDesc);

			NRI.SwapChainPresent(*m_swapChain);
			NRI.QueueSignal(*m_commandQueue, *m_frameFence, 1 + frameIndex);
			*/
		}

	private:
		NRIInterface NRI = {};
		glm::uvec2 m_resolution;
		nri::Device* m_device = nullptr;
		nri::SwapChain* m_swapChain = nullptr;
		nri::CommandQueue* m_commandQueue = nullptr;
		nri::Fence* m_frameFence = nullptr;
		nri::DescriptorPool* m_descriptorPool = {};

		nri::Pipeline* m_pipeline = {};
		nri::PipelineLayout* m_pipelineLayout = {};
		
		nri::Buffer* m_constantBuffer = {};
		nri::Buffer* m_geometryBuffer = {};

		nri::DescriptorSet* m_textureDescriptorSet = {};
		nri::Descriptor* m_sampler = {};

		std::array<Frame, BUFFERED_FRAME_MAX_NUM> m_frames = {};
		std::vector<BackBuffer> m_backBuffers;
		std::vector<nri::Memory*> m_memoryAllocations;
		TextureStoragePtr m_textureStorage;

		uint64_t m_geometryOffset = 0;
		float m_transparency = 1.0f;
		float m_scale = 1.0f;
		nri::Window m_window;
	};


	Simple::Simple(void* hwnd, glm::uvec2 resolution)
		: m_impl(std::make_unique<Impl>(hwnd, resolution))
	{}

	Simple::~Simple() {}

	bool Simple::Init() { return m_impl->Init(); }
	void Simple::Prepare(uint32_t frameIndex) { m_impl->Prepare(frameIndex); }
	void Simple::Render(uint32_t frameIndex) { m_impl->Render(frameIndex); }
	void Simple::SetResolution(glm::uvec2 resolution) { m_impl->SetResolution(resolution); }

} // namespace nwf
