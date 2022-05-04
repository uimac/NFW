CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
project(imgui)

set(IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/imgui)

include_directories(${IMGUI_SOURCE_DIR})

file(GLOB ImGUISrc
    "${IMGUI_SOURCE_DIR}/*.h"
    "${IMGUI_SOURCE_DIR}/*.cpp"
    "${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_SOURCE_DIR}/backends/imgui_impl_dx12.cpp"
)

# VCのフィルター設定
source_group("src" FILES ${ImGUISrc})

# static libraryとする
add_library(imgui STATIC ${ImGUISrc})

# デバッグ時の作業ディレクトリ
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${NFWOutputDir}/${CMAKE_CFG_INTDIR})

# デバッグ版でも外部ライブラリのpdbは使わない
# set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
