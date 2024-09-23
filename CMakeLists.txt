CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
project(NFW)

# パッケージ検索
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# git submodule update --init されていなかったらする
find_package(Git)

if(${GIT_FOUND} AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

# コンパイルフラグ
set(CMAKE_CXX_FLAGS "/EHsc /wd4190 /bigobj /MP")
set(CMAKE_CXX_FLAGS_DEBUG "/MD /Od /Z7 ")
set(CMAKE_CXX_FLAGS_RELEASE "/MD ")

# C++17を有効にする
add_compile_options("/std:c++17")

# lib/NRI
option(NRI_STATIC_LIBRARY "" ON)
option (NRI_ENABLE_NONE_SUPPORT "Enable NONE backend" OFF)
set(GLOBAL_BIN_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib/NRI/" CACHE STRING "")
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/NRI EXCLUDE_FROM_ALL)

# lib/glfw
set(GLFW_STANDALONE FALSE)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
set(GLFW_LIBRARY_TYPE "STATIC")
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/glfw EXCLUDE_FROM_ALL)

# lib/imgui
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/imgui EXCLUDE_FROM_ALL)

# lib/glm
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/glm EXCLUDE_FROM_ALL)

# lib/DirectXTex
option(BUILD_TOOLS "" OFF)
option(BUILD_SAMPLE "" OFF)
option(BUILD_DX11 "" OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/DirectXTex EXCLUDE_FROM_ALL)

# プリプロセッサ
add_definitions(
  -DUNICODE
  -D_UNICODE
  -DWIN32_LEAN_AND_MEAN
  -D_CRT_SECURE_NO_WARNINGS
  -DGLFW_EXPOSE_NATIVE_WIN32
  -DNOMINMAX
  -D_USE_MATH_DEFINES
  -DNDEBUG
  -DGLM_FORCE_DEPTH_ZERO_TO_ONE
  -DGLM_ENABLE_EXPERIMENTAL
)

# MinSizeRelとRelWithDebInfoの選択肢を抑制
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited configs" FORCE)
# ZeroCheck不要
set(CMAKE_SUPPRESS_REGENERATION true)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# WindowsSDK
if("${SHORT_VERSION}" STREQUAL "10.0")
  message(STATUS "Targeting Windows 10. Setting Extensions to version ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
  set_property(TARGET ${EXE_NAME} PROPERTY VS_DESKTOP_EXTENSIONS_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()
if (EXISTS "C:/Program\ Files\ (x86)/Windows\ Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64")
	set(DXSDK_LIBRARIES "C:/Program\ Files\ (x86)/Windows\ Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64")
endif()

if(NOT EXISTS ${DXSDK_LIBRARIES})
	message(FATAL_ERROR "Not found WindowsSDK. Try adding entry Name:DXSDK_LIBRARIES，Type:PATH，Value: C:/ProgramFiles/WindowsSDKVersion/um/x64")
endif()

# Shaderコンパイル
option(DISABLE_SHADER_COMPILATION "disable compilation of shaders" OFF)
if (NOT DISABLE_SHADER_COMPILATION)
    set(SHADER_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/src/shaders")
    set(SHADER_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin/shaders")
    set(DXC_CUSTOM_PATH "${CMAKE_SOURCE_DIR}/tool/dxc_2022_07_13/bin/x64/")

    file(MAKE_DIRECTORY ${SHADER_OUTPUT_PATH})
    include(cmake/ShaderCompilation.cmake)

    # Define build rules for the shaders
    file(GLOB_RECURSE HLSL_FILES "src/shaders/*.hlsl" "src/shaders/*.hlsli")
    list_hlsl_headers("${HLSL_FILES}" HEADER_FILES)
    list_hlsl_shaders("${HLSL_FILES}" "${HEADER_FILES}" SHADER_FILES)
    add_custom_target(NFW_Shaders ALL DEPENDS ${SHADER_FILES} SOURCES "${HEADER_FILES}")
endif()

# UTF8
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 出力先の設定
get_filename_component(NFWOutputDir
    ${CMAKE_CURRENT_LIST_DIR}/bin
    ABSOLUTE
)
set(EXECUTABLE_OUTPUT_PATH ${NFWOutputDir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NFWOutputDir})

# Installを常に走らせる
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)

# src追加
add_subdirectory(src)

# 依存関係
add_dependencies(NFW imgui)
add_dependencies(NFW glfw)
add_dependencies(NFW NRI)
add_dependencies(NFW glm)
add_dependencies(NFW DirectXTex)
add_dependencies(NFW NFW_Shaders)

# スタートアッププロジェクトの設定
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "NFW")
