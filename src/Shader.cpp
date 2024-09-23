#include "Shader.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <wrl.h>

namespace nfw
{
	namespace fs = std::filesystem;

	struct ShaderExt {
		const char* ext;
		nri::StageBits stage;
	};

	// NRIDesc.h  ShaderStage に対応した拡張子
	constexpr std::array<ShaderExt, 13> ShaderExts = { {
		{"", nri::StageBits::NONE},
		{".vs.", nri::StageBits::VERTEX_SHADER},
		{".tcs.", nri::StageBits::TESS_CONTROL_SHADER},
		{".tes.", nri::StageBits::TESS_EVALUATION_SHADER},
		{".gs.", nri::StageBits::GEOMETRY_SHADER},
		{".fs.", nri::StageBits::FRAGMENT_SHADER},
		{".cs.", nri::StageBits::COMPUTE_SHADER},
		{".rgen.", nri::StageBits::RAYGEN_SHADER},
		{".rmiss.", nri::StageBits::MISS_SHADER},
		{"<noimpl>", nri::StageBits::INTERSECTION_SHADER},
		{".rchit.", nri::StageBits::CLOSEST_HIT_SHADER},
		{".rahit.", nri::StageBits::ANY_HIT_SHADER},
		{"<noimpl>", nri::StageBits::CALLABLE_SHADER},
	} };

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
		
			nri::StageBits stage = nri::StageBits::NONE;
			if (shaderPath.find(".vs") != std::string::npos)
			{
				stage = nri::StageBits::VERTEX_SHADER;
			}
			else if (shaderPath.find(".fs") != std::string::npos)
			{
				stage = nri::StageBits::FRAGMENT_SHADER;
			}

			for (uint32_t i = 1, size = static_cast<uint32_t>(ShaderExts.size()); i < size; i++)
			{
				if (path.rfind(ShaderExts[i].ext) != std::string::npos)
				{
					std::ifstream ifs(path, std::ios::in | std::ios::binary);
					std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
					m_shaderData.resize(str.size());
					memcpy(&m_shaderData[0], &str[0], str.size());

					m_shaderDesc.stage = ShaderExts[i].stage;
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
