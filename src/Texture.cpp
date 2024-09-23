#include "Texture.h"
#include <DirectXTex.h>
#include <filesystem>
#include <Extensions/NRIWrapperD3D12.h>


namespace nfw
{
	namespace fs = std::filesystem;

	class Texture::Impl
	{
	public:
		Impl() {}
		~Impl() 
		{
		}

		bool LoadFromFile(const std::string& texturePath)
		{
			fs::path filePath = texturePath;

			DirectX::TexMetadata metaData;
			DirectX::ScratchImage scratch;
			if (DirectX::LoadFromWICFile(filePath.wstring().c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_NONE, &metaData, scratch) == S_OK)
			{

				const DirectX::Image* scratchImage = scratch.GetImage(0, 0, 0);
				if (DirectX::GenerateMipMaps(*scratchImage, DirectX::TEX_FILTER_LINEAR, 0, m_image, false) == S_OK)
				{
					const DirectX::TexMetadata& texMeta = m_image.GetMetadata();
					m_textureDesc.type = nri::TextureType::TEXTURE_2D;
					m_textureDesc.format = nri::nriConvertDXGIFormatToNRI(texMeta.format);
					m_textureDesc.usageMask = nri::TextureUsageBits::SHADER_RESOURCE;
					m_textureDesc.width = texMeta.width;
					m_textureDesc.height = texMeta.height;
					m_textureDesc.depth = 1;
					m_textureDesc.mipNum = texMeta.mipLevels;
					m_textureDesc.layerNum = texMeta.arraySize;
					m_textureDesc.sampleNum = 1;
					return true;
				}
			}

			return false;
		}

		nri::Result CreateTexture(NRIInterface & NRI, nri::Device & device)
		{
			nri::Result res = NRI.CreateTexture(device, m_textureDesc, m_texture);
			if (res == nri::Result::SUCCESS)
			{
				m_texture2DViewDesc = { m_texture, nri::Texture2DViewType::SHADER_RESOURCE_2D, m_textureDesc.format };
				
				for (uint32_t mip = 0; mip < m_textureDesc.mipNum; mip++)
				{
					nri::TextureSubresourceUploadDesc& subresource = m_subresources[mip];
					auto* image = m_image.GetImage(mip, 0, 0);
					subresource.slices = image->pixels;
					subresource.sliceNum = 1;
					subresource.rowPitch = (uint32_t)image->rowPitch;
					subresource.slicePitch = (uint32_t)image->slicePitch;
				}

				m_uploadDesc.subresources = m_subresources.data();
				m_uploadDesc.texture = m_texture;
				m_uploadDesc.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
			}
			return res;
		}

		nri::Result CreateTexture2DView(NRIInterface& NRI, nri::Descriptor** textureShaderDescriptor)
		{
			return NRI.CreateTexture2DView(m_texture2DViewDesc, *textureShaderDescriptor);
		}

		nri::Texture* GetTexture() { return m_texture; }

		nri::TextureDesc GetTextureDesc() const { return m_textureDesc; }

		nri::Texture2DViewDesc GetTexture2DViewDesc() const { return m_texture2DViewDesc; }

		nri::TextureUploadDesc GetTextureUploadDesc() const { return m_uploadDesc; }

	private:
		DirectX::ScratchImage m_image;

		nri::Texture* m_texture = {};
		nri::TextureDesc m_textureDesc{};
		nri::Texture2DViewDesc m_texture2DViewDesc{};
		nri::TextureUploadDesc m_uploadDesc = {};
		std::array<nri::TextureSubresourceUploadDesc, 16> m_subresources;
	};

	// constructor
	Texture::Texture()
		: m_impl(std::make_unique<Impl>())
	{
	}

	// destructor
	Texture::~Texture()
	{
	}

	bool Texture::LoadFromFile(const std::string& texturePath)
	{
		return m_impl->LoadFromFile(texturePath);
	}

	nri::Texture* Texture::GetTexture()  { return m_impl->GetTexture(); }

	nri::TextureDesc Texture::GetTextureDesc() const { return m_impl->GetTextureDesc(); }

	nri::Texture2DViewDesc Texture::GetTexture2DViewDesc() const { return m_impl->GetTexture2DViewDesc(); }

	nri::Result Texture::CreateTexture(NRIInterface& NRI, nri::Device& device) { return m_impl->CreateTexture(NRI, device); }

	nri::Result Texture::CreateTexture2DView(NRIInterface& NRI, nri::Descriptor** textureShaderDescriptor) { return m_impl->CreateTexture2DView(NRI, textureShaderDescriptor); }

	nri::TextureUploadDesc Texture::GetTextureUploadDesc() const { return m_impl->GetTextureUploadDesc(); }

} // namespace nfw
