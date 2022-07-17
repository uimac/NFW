#include "ShaderStorage.h"
#include "Shader.h"

namespace nfw
{
	class ShaderStorage::Impl
	{
	public:
		Impl() {}
		~Impl() {}

		ShaderConstPtr LoadShaderFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath)
		{
			ShaderPtr shader = std::make_shared<Shader>();
			if (shader->LoadFromFile(graphicsAPI, shaderPath))
			{
				m_shaderStorage.push_back(shader);
				return shader;
			}
			return nullptr;
		}

	private:
		std::vector<ShaderConstPtr> m_shaderStorage;
	};

	// constructor
	ShaderStorage::ShaderStorage()
		: m_impl(std::make_unique<Impl>())
	{
	}

	// destructor
	ShaderStorage::~ShaderStorage()
	{
	}

	ShaderConstPtr ShaderStorage::LoadShaderFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath)
	{
		return m_impl->LoadShaderFromFile(graphicsAPI, shaderPath);
	}

} // namespace nfw
