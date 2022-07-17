#pragma once

#include "Api.h"
#include "Types.h"

namespace nfw
{
	class ShaderStorage
	{
		DISALLOW_COPY_AND_ASSIGN(ShaderStorage);
	public:
		ShaderStorage();
		~ShaderStorage();

		ShaderConstPtr LoadShaderFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath);

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
} // namespace nfw
