#pragma once

#include "Api.h"
#include "Types.h"

namespace nfw
{
	class Geometry
	{
		DISALLOW_COPY_AND_ASSIGN(Geometry);
	public:
		Geometry();
		~Geometry();

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
} // namespace nfw

