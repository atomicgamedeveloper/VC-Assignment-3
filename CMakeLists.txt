# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vcpkg toolchain (Windows path as in your setup)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

# ---- dependencies ----
find_package(OpenCV REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

# ---- common shader dirs ----
# Part02 shaders (already used via SHADER_DIR for redTriangle)
set(P2_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenGLIntro/OpenGLIntro/Part02")
# Part03 shaders used by both MainExec and webcamQuad
set(P3_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenGLIntro/OpenGLIntro/Part03")

# ---- executables ----

# ---------- MainExec ----------
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
# Provide an absolute shader directory macro for robust loading
target_compile_definitions(MainExec PRIVATE SHADER_DIR_P3="${P3_SHADER_DIR}/")

target_link_libraries(MainExec PRIVATE
    glfw
    glad::glad
    glm::glm
    ${OpenCV_LIBS}
)

# Copy Part03 shaders next to MainExec after build (so runtime relative paths also work)
set(P3_SHADERS
    "${P3_SHADER_DIR}/videoTextureShader.vert"
    "${P3_SHADER_DIR}/videoTextureShader.frag"
    "${P3_SHADER_DIR}/greyscaleShader.vert"
    "${P3_SHADER_DIR}/greyscaleShader.frag"
    "${P3_SHADER_DIR}/pixelatedShader.vert"
    "${P3_SHADER_DIR}/pixelatedShader.frag"
    "${P3_SHADER_DIR}/blurShader.vert"
    "${P3_SHADER_DIR}/blurShader.frag"
)
add_custom_command(TARGET MainExec POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${P3_SHADERS} $<TARGET_FILE_DIR:MainExec>
)

# ---------- simpleWindow ----------
add_executable(simpleWindow "OpenGLIntro/OpenGLIntro/Part01/simpleWindow.cpp")
target_link_libraries(simpleWindow PRIVATE glfw glad::glad glm::glm)

# ---------- redTriangle ----------
add_executable(redTriangle
    OpenGLIntro/OpenGLIntro/Part02/redTriangle.cpp
    common/Shader.cpp
    common/Camera.cpp
    common/Scene.cpp
    common/Object.cpp
    common/Triangle.cpp
    common/Quad.cpp
    common/ColorShader.cpp
)
target_include_directories(redTriangle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(redTriangle PRIVATE GLM_ENABLE_EXPERIMENTAL)
# Point code at Part02 shader directory for compile-time absolute paths
target_compile_definitions(redTriangle PRIVATE SHADER_DIR="${P2_SHADER_DIR}/")
target_link_libraries(redTriangle PRIVATE glfw glad::glad glm::glm)

# ---------- webcamQuad ----------
add_executable(webcamQuad
    OpenGLIntro/OpenGLIntro/Part03/webcamQuad.cpp
    common/Shader.cpp
    common/Camera.cpp
    common/Scene.cpp
    common/Object.cpp
    common/Quad.cpp
    common/Texture.cpp
    common/TextureShader.cpp
)
target_include_directories(webcamQuad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(webcamQuad PRIVATE GLM_ENABLE_EXPERIMENTAL SHADER_DIR_P3="${P3_SHADER_DIR}/")
target_link_libraries(webcamQuad PRIVATE glfw glad::glad glm::glm ${OpenCV_LIBS})

# Keep the copy step you already had for webcamQuad
add_custom_command(TARGET webcamQuad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${P3_SHADERS} $<TARGET_FILE_DIR:webcamQuad>
)
