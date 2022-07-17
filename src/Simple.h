#include <memory>
#include <glm/glm.hpp>

namespace nwf
{
	class Simple
	{
	public:
		Simple(void* hwnd, glm::uvec2 resolution);
		~Simple();

		bool Init();
		void Prepare(uint32_t frameIndex);
		void Render(uint32_t frameIndex);
		void SetResolution(glm::uvec2 resolution);

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
