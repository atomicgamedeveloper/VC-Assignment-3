cmake_minimum_required(VERSION 3.16)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file")

project(MyProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

set(Stb_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")

set(CUSTOM_SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/customShaders")

file(GLOB SHADER_FILES
    "${CUSTOM_SHADERS_DIR}/*.frag"
    "${CUSTOM_SHADERS_DIR}/*.vert"
)

set(COMMON_SOURCES
    src/common/Camera.cpp
    src/common/Scene.cpp
    src/common/Object.cpp
    src/common/Quad.cpp
    src/common/Shader.cpp
    src/common/Texture.cpp
    src/common/TextureShader.cpp
)

add_executable(MainExec
    src/App.cpp
    ${COMMON_SOURCES}
    "src/AppUtilities.cpp")

add_executable(ExperimentsExec
    src/Experiments.cpp
    ${COMMON_SOURCES}
    "src/AppUtilities.cpp")

add_executable(Calibration
    src/Calibration.cpp
    ${COMMON_SOURCES})

add_executable(makeCharucoBoard
    src/makeCharucoBoard.cpp
)

add_executable(testGLFW
    src/testGLFW.cpp
)

target_include_directories(makeCharucoBoard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(makeCharucoBoard PRIVATE
    ${OpenCV_LIBS}
)

foreach(target MainExec ExperimentsExec Calibration testGLFW)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Stb_INCLUDE_DIR}
    )

    target_compile_definitions(${target} PRIVATE
        GLM_ENABLE_EXPERIMENTAL
        SHADERS_DIR="${CUSTOM_SHADERS_DIR}/"
    )

    target_link_libraries(${target} PRIVATE
        glfw
        glad::glad
        glm::glm
        ${OpenCV_LIBS}
    )

    if(SHADER_FILES)
        foreach(shader_file ${SHADER_FILES})
            add_custom_command(
                TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${shader_file}"
                    "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Copying ${shader_file}"
            )
        endforeach()
    endif()
endforeach()