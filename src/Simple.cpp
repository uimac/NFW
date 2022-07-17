#include "Simple.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include "ShaderStorage.h"
#include "Shader.h"
#include "TextureStorage.h"
#include "Texture.h"

namespace nfw
{
	constexpr nri::Color<float> COLOR_0 = { 1.0f, 1.0f, 0.0f, 1.0f };
	constexpr nri::Color<float> COLOR_1 = { 0.46f, 0.72f, 0.0f, 1.0f };

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
		nri::DeviceSemaphore* deviceSemaphore;
		nri::CommandAllocator* commandAllocator;
		nri::CommandBuffer* commandBuffer;
		nri::Descriptor* constantBufferView;
		nri::DescriptorSet* constantBufferDescriptorSet;
		uint64_t constantBufferViewOffset;
	};

	struct BackBuffer
	{
		nri::FrameBuffer* frameBuffer;
		nri::FrameBuffer* frameBufferUI;
		nri::Descriptor* colorAttachment;
		nri::Texture* texture;
	};

	constexpr uint32_t BUFFERED_FRAME_MAX_NUM = 2;
	constexpr uint32_t SWAP_CHAIN_TEXTURE_NUM = BUFFERED_FRAME_MAX_NUM;

	bool FindPhysicalDeviceGroup(nri::PhysicalDeviceGroup& dstPhysicalDeviceGroup)
	{
		uint32_t deviceGroupNum = 0;
		nri::Result result = nri::GetPhysicalDevices(nullptr, deviceGroupNum);

		if (deviceGroupNum == 0 || result != nri::Result::SUCCESS)
		{
			return false;
		}

		std::vector<nri::PhysicalDeviceGroup> groups(deviceGroupNum);
		result = nri::GetPhysicalDevices(groups.data(), deviceGroupNum);

		if (result != nri::Result::SUCCESS)
		{
			return false;
		}

		size_t groupIndex = 0;
		for (; groupIndex < groups.size(); groupIndex++)
		{
			if (groups[groupIndex].type != nri::PhysicalDeviceType::INTEGRATED)
				break;
		}

		if (groupIndex == groups.size())
		{
			groupIndex = 0;
		}
		dstPhysicalDeviceGroup = groups[groupIndex];

		return true;
	}

	size_t Align(size_t location, size_t align)
	{
		if ((0 == align) || (align & (align - 1)))
		{
			throw std::exception("non-pow2 alignment");
		}
		return ((location + (align - 1)) & ~(align - 1));
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
				NRI.DestroyDeviceSemaphore(*frame.deviceSemaphore);
				NRI.DestroyDescriptor(*frame.constantBufferView);
			}

			for (BackBuffer& backBuffer : m_backBuffers)
			{
				NRI.DestroyFrameBuffer(*backBuffer.frameBuffer);
				NRI.DestroyDescriptor(*backBuffer.colorAttachment);
			}

			NRI.DestroyPipeline(*m_pipeline);
			NRI.DestroyPipelineLayout(*m_pipelineLayout);
			m_textureStorage = nullptr;
			NRI.DestroyDescriptor(*m_sampler);
			NRI.DestroyBuffer(*m_constantBuffer);
			NRI.DestroyBuffer(*m_geometryBuffer);
			NRI.DestroyDescriptorPool(*m_descriptorPool);
			NRI.DestroyQueueSemaphore(*m_acquireSemaphore);
			NRI.DestroyQueueSemaphore(*m_releaseSemaphore);
			NRI.DestroySwapChain(*m_swapChain);

			nri::DestroyDevice(*m_device);
		}

		bool Init()
		{
			nri::GraphicsAPI graphicsAPI = nri::GraphicsAPI::D3D12;
			nri::PhysicalDeviceGroup physicalDeviceGroup = {};
			if (!FindPhysicalDeviceGroup(physicalDeviceGroup)) {
				return false;
			}

			// Device
			nri::DeviceCreationDesc deviceCreationDesc = {};
			deviceCreationDesc.graphicsAPI = graphicsAPI;
			deviceCreationDesc.enableAPIValidation = false;
			deviceCreationDesc.enableNRIValidation = false;
			deviceCreationDesc.D3D11CommandBufferEmulation = false;
			deviceCreationDesc.spirvBindingOffsets = { 100, 200, 300, 400 };
			deviceCreationDesc.physicalDeviceGroup = &physicalDeviceGroup;
			deviceCreationDesc.memoryAllocatorInterface = {};
			NRI_ABORT_ON_FAILURE(nri::CreateDevice(deviceCreationDesc, m_device));

			// NRI
			NRI_ABORT_ON_FAILURE(nri::GetInterface(*m_device, NRI_INTERFACE(nri::CoreInterface), (nri::CoreInterface*)&NRI));
			NRI_ABORT_ON_FAILURE(nri::GetInterface(*m_device, NRI_INTERFACE(nri::SwapChainInterface), (nri::SwapChainInterface*)&NRI));
			NRI_ABORT_ON_FAILURE(nri::GetInterface(*m_device, NRI_INTERFACE(nri::HelperInterface), (nri::HelperInterface*)&NRI));

			// Command queue
			NRI_ABORT_ON_FAILURE(NRI.GetCommandQueue(*m_device, nri::CommandQueueType::GRAPHICS, m_commandQueue));

			// Swap chain
			nri::Format swapChainFormat;
			{
				nri::SwapChainDesc swapChainDesc = {};
				swapChainDesc.windowSystemType = nri::WindowSystemType::WINDOWS;
				swapChainDesc.window = m_window;
				swapChainDesc.commandQueue = m_commandQueue;
				swapChainDesc.format = nri::SwapChainFormat::BT709_G22_8BIT;
				swapChainDesc.verticalSyncInterval = 0;
				swapChainDesc.width = m_resolution.x;
				swapChainDesc.height = m_resolution.y;
				swapChainDesc.textureNum = SWAP_CHAIN_TEXTURE_NUM;
				NRI_ABORT_ON_FAILURE(NRI.CreateSwapChain(*m_device, swapChainDesc, m_swapChain));

				uint32_t swapChainTextureNum;
				nri::Texture* const* swapChainTextures = NRI.GetSwapChainTextures(*m_swapChain, swapChainTextureNum, swapChainFormat);

				for (uint32_t i = 0; i < swapChainTextureNum; i++)
				{
					nri::Texture2DViewDesc textureViewDesc = { swapChainTextures[i], nri::Texture2DViewType::COLOR_ATTACHMENT, swapChainFormat };

					nri::Descriptor* colorAttachment;
					NRI_ABORT_ON_FAILURE(NRI.CreateTexture2DView(textureViewDesc, colorAttachment));

					nri::ClearValueDesc clearColor = {};
					clearColor.rgba32f = COLOR_0;

					nri::FrameBufferDesc frameBufferDesc = {};
					frameBufferDesc.colorAttachmentNum = 1;
					frameBufferDesc.colorAttachments = &colorAttachment;
					frameBufferDesc.colorClearValues = &clearColor;
					nri::FrameBuffer* frameBuffer;
					NRI_ABORT_ON_FAILURE(NRI.CreateFrameBuffer(*m_device, frameBufferDesc, frameBuffer));

					const BackBuffer backBuffer = { frameBuffer, frameBuffer, colorAttachment, swapChainTextures[i] };
					m_backBuffers.push_back(backBuffer);
				}
			}

			NRI_ABORT_ON_FAILURE(NRI.CreateQueueSemaphore(*m_device, m_acquireSemaphore));
			NRI_ABORT_ON_FAILURE(NRI.CreateQueueSemaphore(*m_device, m_releaseSemaphore));

			// Buffered resources
			for (Frame& frame : m_frames)
			{
				NRI_ABORT_ON_FAILURE(NRI.CreateDeviceSemaphore(*m_device, true, frame.deviceSemaphore));
				NRI_ABORT_ON_FAILURE(NRI.CreateCommandAllocator(*m_commandQueue, nri::WHOLE_DEVICE_GROUP, frame.commandAllocator));
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
				descriptorRangeConstant[0] = { 0, 1, nri::DescriptorType::CONSTANT_BUFFER, nri::ShaderStage::ALL };

				nri::DescriptorRangeDesc descriptorRangeTexture[2];
				descriptorRangeTexture[0] = { 0, 1, nri::DescriptorType::TEXTURE, nri::ShaderStage::FRAGMENT };
				descriptorRangeTexture[1] = { 0, 1, nri::DescriptorType::SAMPLER, nri::ShaderStage::FRAGMENT };

				nri::DescriptorSetDesc descriptorSetDescs[] =
				{
					{descriptorRangeConstant, std::size(descriptorRangeConstant)},
					{descriptorRangeTexture, std::size(descriptorRangeTexture)},
				};

				nri::PushConstantDesc pushConstant = { 1, sizeof(float), nri::ShaderStage::FRAGMENT };

				nri::PipelineLayoutDesc pipelineLayoutDesc = {};
				pipelineLayoutDesc.descriptorSetNum = std::size(descriptorSetDescs);
				pipelineLayoutDesc.descriptorSets = descriptorSetDescs;
				pipelineLayoutDesc.pushConstantNum = 1;
				pipelineLayoutDesc.pushConstants = &pushConstant;
				pipelineLayoutDesc.stageMask = nri::PipelineLayoutShaderStageBits::VERTEX | nri::PipelineLayoutShaderStageBits::FRAGMENT;

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
					vertexAttributeDesc[0].offset = 0;
					vertexAttributeDesc[0].d3d = { "POSITION", 0 };
					vertexAttributeDesc[0].vk.location = { 0 };

					vertexAttributeDesc[1].format = nri::Format::RG32_SFLOAT;
					vertexAttributeDesc[1].streamIndex = 0;
					vertexAttributeDesc[1].offset = sizeof(Vertex::position);
					vertexAttributeDesc[1].d3d = { "TEXCOORD", 0 };
					vertexAttributeDesc[1].vk.location = { 1 };
				}

				nri::InputAssemblyDesc inputAssemblyDesc = {};
				inputAssemblyDesc.topology = nri::Topology::TRIANGLE_LIST;
				inputAssemblyDesc.attributes = vertexAttributeDesc;
				inputAssemblyDesc.attributeNum = (uint8_t)std::size(vertexAttributeDesc);
				inputAssemblyDesc.streams = &vertexStreamDesc;
				inputAssemblyDesc.streamNum = 1;

				nri::RasterizationDesc rasterizationDesc = {};
				rasterizationDesc.viewportNum = 1;
				rasterizationDesc.fillMode = nri::FillMode::SOLID;
				rasterizationDesc.cullMode = nri::CullMode::NONE;
				rasterizationDesc.sampleNum = 1;
				rasterizationDesc.sampleMask = 0xFFFF;

				nri::ColorAttachmentDesc colorAttachmentDesc = {};
				colorAttachmentDesc.format = swapChainFormat;
				colorAttachmentDesc.colorWriteMask = nri::ColorWriteBits::RGBA;
				colorAttachmentDesc.blendEnabled = true;
				colorAttachmentDesc.colorBlend = { nri::BlendFactor::SRC_ALPHA, nri::BlendFactor::ONE_MINUS_SRC_ALPHA, nri::BlendFunc::ADD };

				nri::OutputMergerDesc outputMergerDesc = {};
				outputMergerDesc.colorNum = 1;
				outputMergerDesc.color = &colorAttachmentDesc;

				ShaderConstPtr vertexShader = shaderStorage.LoadShaderFromFile(deviceDesc.graphicsAPI, "Simple.vs");
				ShaderConstPtr pixelShader = shaderStorage.LoadShaderFromFile(deviceDesc.graphicsAPI, "Simple.fs");

				nri::ShaderDesc shaderStages[] =
				{
					vertexShader->GetShaderDesc(),
					pixelShader->GetShaderDesc(),
				};

				nri::GraphicsPipelineDesc graphicsPipelineDesc = {};
				graphicsPipelineDesc.pipelineLayout = m_pipelineLayout;
				graphicsPipelineDesc.inputAssembly = &inputAssemblyDesc;
				graphicsPipelineDesc.rasterization = &rasterizationDesc;
				graphicsPipelineDesc.outputMerger = &outputMergerDesc;
				graphicsPipelineDesc.shaderStages = shaderStages;
				graphicsPipelineDesc.shaderStageNum = std::size(shaderStages);

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
				samplerDesc.minification = nri::Filter::LINEAR;
				samplerDesc.magnification = nri::Filter::LINEAR;
				samplerDesc.mip = nri::Filter::LINEAR;
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
					&m_textureDescriptorSet, 1, nri::WHOLE_DEVICE_GROUP, 0));

				nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDescs[2] = {};
				descriptorRangeUpdateDescs[0].descriptorNum = 1;
				nri::Descriptor* descriptor = m_textureStorage->GetTextureShaderDescriptor();
				descriptorRangeUpdateDescs[0].descriptors = &descriptor;

				descriptorRangeUpdateDescs[1].descriptorNum = 1;
				descriptorRangeUpdateDescs[1].descriptors = &m_sampler;
				NRI.UpdateDescriptorRanges(*m_textureDescriptorSet, nri::WHOLE_DEVICE_GROUP, 0, std::size(descriptorRangeUpdateDescs), descriptorRangeUpdateDescs);

				// Constant buffer
				for (Frame& frame : m_frames)
				{
					NRI_ABORT_ON_FAILURE(NRI.AllocateDescriptorSets(*m_descriptorPool, *m_pipelineLayout, 0, &frame.constantBufferDescriptorSet, 1, nri::WHOLE_DEVICE_GROUP, 0));

					nri::DescriptorRangeUpdateDesc descriptorRangeUpdateDesc = { &frame.constantBufferView, 1 };
					NRI.UpdateDescriptorRanges(*frame.constantBufferDescriptorSet, nri::WHOLE_DEVICE_GROUP, 0, 1, &descriptorRangeUpdateDesc);
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
				bufferData.nextAccess = nri::AccessBits::INDEX_BUFFER | nri::AccessBits::VERTEX_BUFFER;

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
			const uint32_t windowWidth = m_resolution.x;
			const uint32_t windowHeight = m_resolution.y;
			const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;
			const Frame& frame = m_frames[bufferedFrameIndex];

			const uint32_t currentTextureIndex = NRI.AcquireNextSwapChainTexture(*m_swapChain, *m_acquireSemaphore);
			BackBuffer& currentBackBuffer = m_backBuffers[currentTextureIndex];

			NRI.WaitForSemaphore(*m_commandQueue, *frame.deviceSemaphore);
			NRI.ResetCommandAllocator(*frame.commandAllocator);

			ConstantBufferLayout* constants = (ConstantBufferLayout*)NRI.MapBuffer(*m_constantBuffer, frame.constantBufferViewOffset, sizeof(ConstantBufferLayout));
			if (constants)
			{
				constants->color[0] = 1.0f;
				constants->color[1] = 1.0f;
				constants->color[2] = 1.0f;
				constants->scale = m_scale;

				NRI.UnmapBuffer(*m_constantBuffer);
			}

			nri::TextureTransitionBarrierDesc textureTransitionBarrierDesc = {};
			textureTransitionBarrierDesc.texture = currentBackBuffer.texture;
			textureTransitionBarrierDesc.prevAccess = nri::AccessBits::UNKNOWN;
			textureTransitionBarrierDesc.nextAccess = nri::AccessBits::COLOR_ATTACHMENT;
			textureTransitionBarrierDesc.prevLayout = nri::TextureLayout::UNKNOWN;
			textureTransitionBarrierDesc.nextLayout = nri::TextureLayout::COLOR_ATTACHMENT;
			textureTransitionBarrierDesc.arraySize = 1;
			textureTransitionBarrierDesc.mipNum = 1;

			nri::CommandBuffer* commandBuffer = frame.commandBuffer;
			NRI.BeginCommandBuffer(*commandBuffer, m_descriptorPool, 0);
			{
				nri::TransitionBarrierDesc transitionBarriers = {};
				transitionBarriers.textureNum = 1;
				transitionBarriers.textures = &textureTransitionBarrierDesc;
				NRI.CmdPipelineBarrier(*commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);

				NRI.CmdBeginRenderPass(*commandBuffer, *currentBackBuffer.frameBuffer, nri::RenderPassBeginFlag::NONE);
				{
					{
						//helper::Annotation annotation(NRI, *commandBuffer, "Clear");

						uint32_t halfWidth = windowWidth / 2;
						uint32_t halfHeight = windowHeight / 2;

						nri::ClearDesc clearDesc = {};
						clearDesc.colorAttachmentIndex = 0;
						clearDesc.value.rgba32f = COLOR_1;
						nri::Rect rects[2];
						rects[0] = { 0, 0, halfWidth, halfHeight };
						rects[1] = { (int32_t)halfWidth, (int32_t)halfHeight, halfWidth, halfHeight };
						NRI.CmdClearAttachments(*commandBuffer, &clearDesc, 1, rects, std::size(rects));
					}

					{
						//helper::Annotation annotation(NRI, *commandBuffer, "Triangle");

						const nri::Viewport viewport = { 0.0f, 0.0f, (float)windowWidth, (float)windowHeight, 0.0f, 1.0f };
						NRI.CmdSetViewports(*commandBuffer, &viewport, 1);

						NRI.CmdSetPipelineLayout(*commandBuffer, *m_pipelineLayout);
						NRI.CmdSetPipeline(*commandBuffer, *m_pipeline);
						NRI.CmdSetConstants(*commandBuffer, 0, &m_transparency, 4);
						NRI.CmdSetIndexBuffer(*commandBuffer, *m_geometryBuffer, 0, nri::IndexType::UINT16);
						NRI.CmdSetVertexBuffers(*commandBuffer, 0, 1, &m_geometryBuffer, &m_geometryOffset);

						nri::DescriptorSet* sets[2] = { frame.constantBufferDescriptorSet, m_textureDescriptorSet };
						NRI.CmdSetDescriptorSets(*commandBuffer, 0, std::size(sets), sets, nullptr);

						nri::Rect scissor = { 0, 0, windowWidth, windowHeight };
						NRI.CmdSetScissors(*commandBuffer, &scissor, 1);
						NRI.CmdDrawIndexed(*commandBuffer, 6, 1, 0, 0, 0);
					}

					//RenderUserInterface(*commandBuffer);
				}
				NRI.CmdEndRenderPass(*commandBuffer);

				textureTransitionBarrierDesc.prevAccess = textureTransitionBarrierDesc.nextAccess;
				textureTransitionBarrierDesc.nextAccess = nri::AccessBits::UNKNOWN;
				textureTransitionBarrierDesc.prevLayout = textureTransitionBarrierDesc.nextLayout;
				textureTransitionBarrierDesc.nextLayout = nri::TextureLayout::PRESENT;

				NRI.CmdPipelineBarrier(*commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);
			}
			NRI.EndCommandBuffer(*commandBuffer);

			nri::WorkSubmissionDesc workSubmissionDesc = {};
			workSubmissionDesc.commandBufferNum = 1;
			workSubmissionDesc.commandBuffers = &commandBuffer;
			workSubmissionDesc.wait = &m_acquireSemaphore;
			workSubmissionDesc.waitNum = 1;
			workSubmissionDesc.signal = &m_releaseSemaphore;
			workSubmissionDesc.signalNum = 1;
			NRI.SubmitQueueWork(*m_commandQueue, workSubmissionDesc, frame.deviceSemaphore);

			NRI.SwapChainPresent(*m_swapChain, *m_releaseSemaphore);
		}

		void Render2(uint32_t frameIndex)
		{
			const uint32_t windowWidth = m_resolution.x;
			const uint32_t windowHeight = m_resolution.y;
			const uint32_t bufferedFrameIndex = frameIndex % BUFFERED_FRAME_MAX_NUM;
			const Frame& frame = m_frames[bufferedFrameIndex];

			const uint32_t backBufferIndex = NRI.AcquireNextSwapChainTexture(*m_swapChain, *m_acquireSemaphore);
			const BackBuffer& backBuffer = m_backBuffers[backBufferIndex];

			NRI.WaitForSemaphore(*m_commandQueue, *frame.deviceSemaphore);
			NRI.ResetCommandAllocator(*frame.commandAllocator);

			nri::CommandBuffer& commandBuffer = *frame.commandBuffer;
			NRI.BeginCommandBuffer(commandBuffer, nullptr, 0);
			{
				nri::TextureTransitionBarrierDesc textureTransitionBarrierDesc = {};
				textureTransitionBarrierDesc.texture = backBuffer.texture;
				textureTransitionBarrierDesc.prevAccess = nri::AccessBits::UNKNOWN;
				textureTransitionBarrierDesc.nextAccess = nri::AccessBits::COLOR_ATTACHMENT;
				textureTransitionBarrierDesc.prevLayout = nri::TextureLayout::UNKNOWN;
				textureTransitionBarrierDesc.nextLayout = nri::TextureLayout::COLOR_ATTACHMENT;
				textureTransitionBarrierDesc.arraySize = 1;
				textureTransitionBarrierDesc.mipNum = 1;

				nri::TransitionBarrierDesc transitionBarriers = {};
				transitionBarriers.textureNum = 1;
				transitionBarriers.textures = &textureTransitionBarrierDesc;
				NRI.CmdPipelineBarrier(commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);

				NRI.CmdBeginRenderPass(commandBuffer, *backBuffer.frameBuffer, nri::RenderPassBeginFlag::NONE);
				{
					nri::ClearDesc clearDesc = {};
					clearDesc.colorAttachmentIndex = 0;

					clearDesc.value.rgba32f = { 0.0f, 90.0f / 255.0f, 187.0f / 255.0f, 1.0f };
					nri::Rect rect1 = { 0, 0, windowWidth, windowHeight / 2 };
					NRI.CmdClearAttachments(commandBuffer, &clearDesc, 1, &rect1, 1);

					clearDesc.value.rgba32f = { 1.0f, 213.0f / 255.0f, 0.0f, 1.0f };
					nri::Rect rect2 = { 0, (int32_t)windowHeight / 2, windowWidth, windowHeight / 2 };
					NRI.CmdClearAttachments(commandBuffer, &clearDesc, 1, &rect2, 1);
				}
				NRI.CmdEndRenderPass(commandBuffer);

				textureTransitionBarrierDesc.prevAccess = textureTransitionBarrierDesc.nextAccess;
				textureTransitionBarrierDesc.nextAccess = nri::AccessBits::UNKNOWN;
				textureTransitionBarrierDesc.prevLayout = textureTransitionBarrierDesc.nextLayout;
				textureTransitionBarrierDesc.nextLayout = nri::TextureLayout::PRESENT;

				NRI.CmdPipelineBarrier(commandBuffer, &transitionBarriers, nullptr, nri::BarrierDependency::ALL_STAGES);
			}
			NRI.EndCommandBuffer(commandBuffer);

			const nri::CommandBuffer* commandBufferArray[] = { &commandBuffer };

			nri::WorkSubmissionDesc workSubmissionDesc = {};
			workSubmissionDesc.commandBufferNum = std::size(commandBufferArray);
			workSubmissionDesc.commandBuffers = commandBufferArray;
			workSubmissionDesc.wait = &m_acquireSemaphore;
			workSubmissionDesc.waitNum = 1;
			workSubmissionDesc.signal = &m_releaseSemaphore;
			workSubmissionDesc.signalNum = 1;

			NRI.SubmitQueueWork(*m_commandQueue, workSubmissionDesc, frame.deviceSemaphore);
			NRI.SwapChainPresent(*m_swapChain, *m_releaseSemaphore);
		}

	private:
		NRIInterface NRI = {};
		glm::uvec2 m_resolution;
		nri::Device* m_device = nullptr;
		nri::SwapChain* m_swapChain = nullptr;
		nri::CommandQueue* m_commandQueue = nullptr;
		nri::QueueSemaphore* m_acquireSemaphore = nullptr;
		nri::QueueSemaphore* m_releaseSemaphore = nullptr;
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
