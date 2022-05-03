CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
project(NFW)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB NFWSrc
    "*.h"
    "*.cpp"
)

# VCのフィルター設定
source_group("src" FILES ${NFWSrc})

# exeにする設定
add_executable(NFW ${NFWSrc})


# デバッグ時の作業ディレクトリ
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${NFWOutputDir}/${CMAKE_CFG_INTDIR})

# デバッグ版でも外部ライブラリのpdbは使わない
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")