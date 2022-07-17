#include <iostream>
#include <thread>
#include <chrono>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Simple.h"

int main()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "NFW", nullptr, nullptr);

	using namespace nwf;
	Simple* simple = new Simple(glfwGetWin32Window(window), {800, 600});
	simple->Init();

	uint32_t i = 0;
	while (!glfwWindowShouldClose(window))
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(0));

		glfwPollEvents();

		bool isActive = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
		if (!isActive)
		{
			i--;
			continue;
		}

		simple->Prepare(i);
		simple->Render(i);
		++i;
	}

	delete simple;
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

