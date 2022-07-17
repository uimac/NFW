#pragma once

#include "Api.h"
#include "Types.h"

namespace nfw
{
	class Texture
	{
		DISALLOW_COPY_AND_ASSIGN(Texture);
	public:
		Texture();
		~Texture();

		bool LoadFromFile(const std::string& texturePath);

		nri::Texture* Texture::GetTexture();

		nri::TextureDesc GetTextureDesc() const;
		nri::Texture2DViewDesc GetTexture2DViewDesc() const;
		nri::TextureUploadDesc GetTextureUploadDesc() const;

		nri::Result CreateTexture(NRIInterface& NRI, nri::Device& device);
		nri::Result CreateTexture2DView(NRIInterface& NRI, nri::Descriptor** textureShaderResource);

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
} // namespace nfw
