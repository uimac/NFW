#include "Geometry.h"

namespace nfw
{
	class Geometry::Impl
	{
	public:
		Impl() {}
		~Impl() {}

	};

	// constructor
	Geometry::Geometry()
		: m_impl(std::make_unique<Impl>())
	{
	}

	// destructor
	Geometry::~Geometry()
	{
	}

} // namespace nfw
