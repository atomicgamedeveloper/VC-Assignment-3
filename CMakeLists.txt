# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vcpkg toolchain (Windows path as in your setup)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

find_package(OpenCV REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

set(CUSTOM_SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/customShaders")

add_executable(MainExec
    main.cpp
    common/Shader.cpp
    common/Camera.cpp
    common/Scene.cpp
    common/Object.cpp
    common/Quad.cpp
    common/Texture.cpp
    common/TextureShader.cpp
)
target_include_directories(MainExec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(MainExec PRIVATE GLM_ENABLE_EXPERIMENTAL)
target_compile_definitions(MainExec PRIVATE SHADERS_DIR="${CUSTOM_SHADERS_DIR}/")

target_link_libraries(MainExec PRIVATE
    glfw
    glad::glad
    glm::glm
    ${OpenCV_LIBS}
)

set(CUSTOM_SHADERS_DIR
    "${CUSTOM_SHADERS_DIR}/videoTextureShader.vert"
    "${CUSTOM_SHADERS_DIR}/videoTextureShader.frag"
    "${CUSTOM_SHADERS_DIR}/greyscaleShader.frag"
    "${CUSTOM_SHADERS_DIR}/pixelatedShader.frag"
    "${CUSTOM_SHADERS_DIR}/blurShader.frag"
    "${CUSTOM_SHADERS_DIR}/sinCityShader.frag"
)
add_custom_command(TARGET MainExec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CUSTOM_SHADERS_DIR} $<TARGET_FILE_DIR:MainExec>
)