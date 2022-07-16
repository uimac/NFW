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
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/NRI)

# lib/glfw
set(GLFW_STANDALONE FALSE)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
set(GLFW_LIBRARY_TYPE "STATIC")
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/glfw)

# lib/imgui
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/imgui)

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

# UTF8
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# 出力先の設定
get_filename_component(NFWOutputDir
    ${CMAKE_CURRENT_LIST_DIR}/bin
    ABSOLUTE
)
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
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

# スタートアッププロジェクトの設定
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "NFW")
