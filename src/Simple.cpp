
#include <array>
#include <vector>
#include <map>
#include <thread>
#include <chrono>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <imgui.h>
#include <NRI.h>
#include <Extensions/NRIDeviceCreation.h>
#include <Extensions/NRISwapChain.h>
#include <Extensions/NRIHelper.h>

#include "Simple.h"

#define NRI_ABORT_ON_FAILURE(result) \
    if ((result) != nri::Result::SUCCESS) \
        exit(1);

namespace nwf
{
	struct NRIInterface
		: public nri::CoreInterface
		, public nri::SwapChainInterface
		, public nri::HelperInterface
	{};

	struct Frame
	{
		nri::DeviceSemaphore* deviceSemaphore;
		nri::CommandAllocator* commandAllocator;
		nri::CommandBuffer* commandBuffer;
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

	class Simple::Impl
	{
	public:
		Impl::Impl(void* hwnd)
		{
			m_window.windows.hwnd = hwnd;
		}

		Impl::~Impl()
		{
			NRI.WaitForIdle(*m_commandQueue);

			for (Frame& frame : m_frames)
			{
				NRI.DestroyCommandBuffer(*frame.commandBuffer);
				NRI.DestroyCommandAllocator(*frame.commandAllocator);
				NRI.DestroyDeviceSemaphore(*frame.deviceSemaphore);
			}

			for (BackBuffer& backBuffer : m_backBuffers)
			{
				NRI.DestroyFrameBuffer(*backBuffer.frameBuffer);
				NRI.DestroyDescriptor(*backBuffer.colorAttachment);
			}

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
				swapChainDesc.width = 800;
				swapChainDesc.height = 600;
				swapChainDesc.textureNum = SWAP_CHAIN_TEXTURE_NUM;
				NRI_ABORT_ON_FAILURE(NRI.CreateSwapChain(*m_device, swapChainDesc, m_swapChain));

				uint32_t swapChainTextureNum;
				nri::Texture* const* swapChainTextures = NRI.GetSwapChainTextures(*m_swapChain, swapChainTextureNum, swapChainFormat);

				for (uint32_t i = 0; i < swapChainTextureNum; i++)
				{
					nri::Texture2DViewDesc textureViewDesc = { swapChainTextures[i], nri::Texture2DViewType::COLOR_ATTACHMENT, swapChainFormat };

					nri::Descriptor* colorAttachment;
					NRI_ABORT_ON_FAILURE(NRI.CreateTexture2DView(textureViewDesc, colorAttachment));

					nri::FrameBufferDesc frameBufferDesc = {};
					frameBufferDesc.colorAttachmentNum = 1;
					frameBufferDesc.colorAttachments = &colorAttachment;
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

			return true;
		}

		void SetResolution(uint32_t width, uint32_t height)
		{
			m_width = width;
			m_height = height;
		}

		void Prepare(uint32_t frameIndex)
		{
		}

		void Render(uint32_t frameIndex)
		{
			const uint32_t windowWidth = 800;
			const uint32_t windowHeight = 600;
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
		uint32_t m_width;
		uint32_t m_height;

		NRIInterface NRI = {};
		nri::Device* m_device = nullptr;
		nri::SwapChain* m_swapChain = nullptr;
		nri::CommandQueue* m_commandQueue = nullptr;
		nri::QueueSemaphore* m_acquireSemaphore = nullptr;
		nri::QueueSemaphore* m_releaseSemaphore = nullptr;

		std::array<Frame, BUFFERED_FRAME_MAX_NUM> m_frames = {};
		std::vector<BackBuffer> m_backBuffers;

		nri::Window m_window;
	};


	Simple::Simple(void* hwnd)
		: m_impl(std::make_unique<Impl>(hwnd))
	{}

	Simple::~Simple() {}

	bool Simple::Init() { return m_impl->Init(); }
	void Simple::Prepare(uint32_t frameIndex) { m_impl->Prepare(frameIndex); }
	void Simple::Render(uint32_t frameIndex) { m_impl->Render(frameIndex); }
	void Simple::SetResolution(uint32_t width, uint32_t height) { m_impl->SetResolution(width, height); }

} // namespace nwf
