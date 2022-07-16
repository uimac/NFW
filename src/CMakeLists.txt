CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
project(NFW)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/lib/glfw/include)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui)
include_directories(${CMAKE_SOURCE_DIR}/lib/NRI/include)

file(GLOB NFWSrc
    "*.h"
    "*.cpp"
)

# VCのフィルター設定
source_group("src" FILES ${NFWSrc})

# exeにする設定
add_executable(NFW ${NFWSrc})

# libのリンク
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/cmake/imgui/${CMAKE_CFG_INTDIR}/imgui.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/glfw/src/${CMAKE_CFG_INTDIR}/glfw3.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI_Shared.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI_Validation.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI_VK.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI_D3D12.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_BINARY_DIR}/lib/NRI/${CMAKE_CFG_INTDIR}/NRI_D3D11.lib)
target_link_libraries(NFW PRIVATE ${CMAKE_SOURCE_DIR}/lib/NRI/External/NVAPI/amd64/nvapi64.lib)
target_link_libraries(NFW PRIVATE ${DXSDK_LIBRARIES}/d3d12.lib)
target_link_libraries(NFW PRIVATE ${DXSDK_LIBRARIES}/d3d11.lib)
target_link_libraries(NFW PRIVATE ${DXSDK_LIBRARIES}/dxguid.lib)
target_link_libraries(NFW PRIVATE ${DXSDK_LIBRARIES}/dxgi.lib)
target_link_libraries(NFW PRIVATE ${DXSDK_LIBRARIES}/D3DCompiler.lib)

# デバッグ時の作業ディレクトリ
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${NFWOutputDir}/${CMAKE_CFG_INTDIR})

# デバッグ版でも外部ライブラリのpdbは使わない
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")