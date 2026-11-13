cmake_minimum_required(VERSION 3.20)
project(fnx_test_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ImGui (docking branch optional; using master/stable tag via git). Requires internet+git on first configure.
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_library(imgui_backend STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
  ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_backend PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui_backend PUBLIC UNICODE _UNICODE)

add_executable(fnx_test_ui WIN32
  main.cpp
)

target_link_libraries(fnx_test_ui PRIVATE imgui_backend d3d11 dxgi d3dcompiler dwmapi user32 gdi32 shell32)
if(MSVC)
  target_compile_options(fnx_test_ui PRIVATE /W4 /EHsc)
endif()
