cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid_Unforked LANGUAGES C CXX)

add_subdirectory("lib")

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp
    lib/ImGuiFileDialog/ImGuiFileDialog.cpp
    lib/implot/implot.cpp lib/implot/implot_items.cpp lib/implot/implot_demo.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(
    lib/zep/include
#    lib/zep/extensions
    lib/implot
    lib/ImGuiFileDialog
)

target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI Zep::Zep)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS) # ImVec2 & ImVec4 math operators
add_definitions(-DZEP_USE_SDL) # Needed for ctrl key to work with Zep
