#=================== SDL2 ===================

cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)
add_subdirectory(${SDL2_DIR})

#=================== ImGui ===================

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(IMGUI STATIC)

target_sources(IMGUI
    PRIVATE
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui.cpp

    PRIVATE
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl.cpp
    )

target_include_directories(IMGUI
    PUBLIC ${IMGUI_DIR}
    PUBLIC ${IMGUI_DIR}/backends
    PUBLIC ${SDL2_DIR}/include
    )

find_package(OpenGL REQUIRED)
target_link_libraries(IMGUI PUBLIC ${OPENGL_LIBRARIES})
target_link_libraries(IMGUI PUBLIC SDL2main SDL2 ${CMAKE_DL_LIBS})

#=================== zep ===================
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(FREETYPE_DIR /opt/homebrew/opt/freetype)
find_package(Freetype REQUIRED)

set(BUILD_TESTS OFF CACHE STRING "Make the tests" FORCE)
set(BUILD_DEMOS OFF CACHE STRING "Make the demo app" FORCE)
set(BUILD_EXTENSIONS OFF CACHE STRING "Make the extension library (required for demo)" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zep)
