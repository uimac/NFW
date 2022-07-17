#pragma once

#include "Api.h"
#include "Types.h"

namespace nfw
{
	class Shader
	{
		DISALLOW_COPY_AND_ASSIGN(Shader);
	public:
		Shader();
		~Shader();

		bool LoadFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath);

		nri::ShaderDesc GetShaderDesc() const;

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
} // namespace nfw
