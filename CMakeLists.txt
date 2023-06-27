cmake_minimum_required(VERSION 3.0)
project(hoi4_hax LANGUAGES CXX)

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "" FORCE)

message("VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")

set(CMAKE_CXX_STANDARD 23)    
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spdlog REQUIRED)
find_path(CR_INCLUDE_DIRS "cr.h")
find_package(fmt CONFIG REQUIRED)

add_subdirectory(minhook)

add_library(hoi4_hax SHARED main.cpp memory.cpp kiero.cpp
    imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_dx11.cpp imgui/backends/imgui_impl_win32.cpp imgui/misc/cpp/imgui_stdlib.cpp)
target_link_directories(hoi4_hax PRIVATE "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Lib\\x64")
target_include_directories(hoi4_hax PRIVATE imgui 
    "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Include"
    ${CR_INCLUDE_DIRS})
target_link_libraries(hoi4_hax PRIVATE minhook d3dx11d.lib spdlog::spdlog_header_only fmt::fmt-header-only)

add_library(hoi4_hax_loader SHARED loader/loader.cpp)
target_compile_definitions(hoi4_hax_loader PRIVATE BINARY_PATH="${CMAKE_SOURCE_DIR}/build")
target_link_directories(hoi4_hax_loader PRIVATE "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Lib\\x64")
target_include_directories(hoi4_hax_loader PRIVATE ${CR_INCLUDE_DIRS})
target_link_libraries(hoi4_hax_loader PRIVATE minhook d3dx11d.lib spdlog::spdlog_header_only fmt::fmt-header-only)