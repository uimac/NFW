#pragma once

#include "Api.h"

namespace nfw
{
	struct NRIInterface
		: public nri::CoreInterface
		, public nri::SwapChainInterface
		, public nri::HelperInterface
	{};

	class Texture;
	using TexturePtr = std::shared_ptr<Texture>;
	using TextureConstPtr = std::shared_ptr<const Texture>;

	class TextureStorage;
	using TextureStoragePtr = std::shared_ptr<TextureStorage>;

	class Shader;
	using ShaderPtr = std::shared_ptr<Shader>;
	using ShaderConstPtr = std::shared_ptr<const Shader>;

	class Geometry;
	using GeometryPtr = std::shared_ptr<Geometry>;
	using GeometryConstPtr = std::shared_ptr<const Geometry>;

}
