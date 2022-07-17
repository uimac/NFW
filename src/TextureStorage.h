#pragma once

#include "Api.h"
#include "Types.h"

namespace nfw
{
	class TextureStorage
	{
		DISALLOW_COPY_AND_ASSIGN(TextureStorage);
	public:
		TextureStorage(NRIInterface& NRI);
		~TextureStorage();

		TexturePtr LoadFromFile(const std::string& texturePath);

		nri::Result CreateTexture2DView();

		nri::Descriptor* GetTextureShaderDescriptor() const;

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
} // namespace nfw
