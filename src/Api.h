#pragma once

#ifdef NFWDLL
#define NFWDLL_API __declspec(dllexport)
#else
#define NFWDLL_API __declspec(dllimport)
#endif

// noncopyable
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
		TypeName(const TypeName&) = delete; \
		void operator=(const TypeName&) = delete

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <optional>
#include <functional>
#include <string>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <NRI.h>
#include <Extensions/NRIDeviceCreation.h>
#include <Extensions/NRISwapChain.h>
#include <Extensions/NRIHelper.h>

#define NRI_ABORT_ON_FAILURE(result) \
    if ((result) != nri::Result::SUCCESS) \
        exit(1);
