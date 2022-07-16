#include <memory>

namespace nwf
{
	class Simple
	{
	public:
		Simple(void* hwnd);
		~Simple();

		bool Init();
		void Prepare(uint32_t frameIndex);
		void Render(uint32_t frameIndex);
		void SetResolution(uint32_t width, uint32_t height);

	private:
		class Impl;
		std::unique_ptr<Impl> m_impl;
	};
}
