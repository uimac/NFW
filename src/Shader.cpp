#include "Shader.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <wrl.h>

namespace nfw
{
	namespace fs = std::filesystem;

	// NRIDesc.h  ShaderStage に対応した拡張子
	constexpr std::array<const char*, 13> ShaderExts =
	{
		"",
		".vs.",
		".tcs.",
		".tes.",
		".gs.",
		".fs.",
		".cs.",
		".rgen.",
		".rmiss.",
		"<noimpl>",
		".rchit.",
		".rahit.",
		"<noimpl>"
	};

	const char* GetShaderExt(nri::GraphicsAPI graphicsAPI)
	{
		if (graphicsAPI == nri::GraphicsAPI::D3D11)
			return ".dxbc";
		else if (graphicsAPI == nri::GraphicsAPI::D3D12)
			return ".dxil";

		return ".spirv";
	}

	class Shader::Impl
	{
	public:
		Impl() {}
		~Impl() {}

		nri::ShaderDesc GetShaderDesc() const
		{
			return m_shaderDesc;
		}


		bool LoadFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath)
		{
			const char* ext = GetShaderExt(graphicsAPI);
			std::string path = "../shaders/" + shaderPath + ext;

			for (uint32_t i = 1, size = static_cast<uint32_t>(ShaderExts.size()); i < size; i++)
			{
				if (path.rfind(ShaderExts[i]) != std::string::npos)
				{
					std::ifstream ifs(path, std::ios::in | std::ios::binary);
					std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
					m_shaderData.resize(str.size());
					memcpy(&m_shaderData[0], &str[0], str.size());

					m_shaderDesc.stage = (nri::ShaderStage)i;
					m_shaderDesc.bytecode = m_shaderData.data();
					m_shaderDesc.size = m_shaderData.size();
					m_shaderDesc.entryPointName = nullptr;
					return true;
				}
			}
			return false;
		}

	private:

		std::vector<uint8_t> m_shaderData;
		nri::ShaderDesc m_shaderDesc{};
	};

	// constructor
	Shader::Shader()
		: m_impl(std::make_unique<Impl>())
	{
	}

	// destructor
	Shader::~Shader()
	{
	}

	bool Shader::LoadFromFile(nri::GraphicsAPI graphicsAPI, const std::string& shaderPath)
	{
		return m_impl->LoadFromFile(graphicsAPI, shaderPath);
	}

	nri::ShaderDesc Shader::GetShaderDesc() const
	{
		return m_impl->GetShaderDesc();
	}

} // namespace nfw
