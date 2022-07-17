#include "TextureStorage.h"
#include "Texture.h"

namespace nfw
{
	class TextureStorage::Impl
	{
	public:
		Impl(NRIInterface& nri)
			: NRI(nri)
		{}
		~Impl() 
		{
			if (m_textureShaderDescriptor)
			{
				NRI.DestroyDescriptor(*m_textureShaderDescriptor);
			}
			for (auto texture : m_textures)
			{
				nri::Texture* tex = texture->GetTexture();
				if (tex)
				{
					NRI.DestroyTexture(*tex);
				}
			}
		}

		TexturePtr LoadFromFile(const std::string& texturePath)
		{
			TexturePtr texture = std::make_shared<Texture>();
			if (texture->LoadFromFile(texturePath))
			{
				m_textures.push_back(texture);
				return texture;
			}
			return nullptr;
		}

		nri::Result CreateTexture2DView()
		{
			for (uint32_t i = 0, size = static_cast<uint32_t>(m_textures.size()); i < size; ++i)
			{
				nri::Result res = m_textures[i]->CreateTexture2DView(NRI, &m_textureShaderDescriptor);
				if (res != nri::Result::SUCCESS)
				{
					return nri::Result::FAILURE;
				}
			}
			return nri::Result::SUCCESS;
		}


		nri::Descriptor* GetTextureShaderDescriptor() const { return m_textureShaderDescriptor; }

	private:
		NRIInterface& NRI;
		std::vector<TexturePtr> m_textures;
		nri::Descriptor* m_textureShaderDescriptor = nullptr;
	};

	// constructor
	TextureStorage::TextureStorage(NRIInterface& NRI)
		: m_impl(std::make_unique<Impl>(NRI))
	{
	}

	// destructor
	TextureStorage::~TextureStorage()
	{
	}

	TexturePtr TextureStorage::LoadFromFile(const std::string& texturePath)
	{
		return m_impl->LoadFromFile(texturePath);
	}

	nri::Result TextureStorage::CreateTexture2DView()
	{
		return m_impl->CreateTexture2DView();
	}

	nri::Descriptor* TextureStorage::GetTextureShaderDescriptor() const { return m_impl->GetTextureShaderDescriptor(); }

} // namespace nfw
