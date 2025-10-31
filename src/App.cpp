#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdio.h>
#include <windows.h>
#include <opencv2/core/utils/logger.hpp> 
#include <stdlib.h>
#include <string>
#include <map>

// Import utility functions
#include "../hpp/Filters.hpp"
#include "../hpp/InputHandler.hpp"
#include "../hpp/AppUtilities.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <opencv2/opencv.hpp>

#include <common/Shader.hpp>
#include <common/Camera.hpp>
#include <common/Scene.hpp>
#include <common/Object.hpp>
#include <common/TextureShader.hpp>
#include <common/Quad.hpp>
#include <common/Texture.hpp>

using namespace std;
using namespace cv;

int main(int argc, char** argv) {
    utils::logging::setLogLevel(utils::logging::LOG_LEVEL_SILENT);
    
    Mat frame;

    VideoCapture cap;

    double fps = cap.get(CAP_PROP_FPS);
    cout << "Frames per second using video.get(CAP_PROP_FPS) : " << fps << endl;

    int deviceID = 0;
    int apiID = CAP_ANY;

    cap.open(deviceID, apiID);

    if (!cap.isOpened()) {
        cerr << "ERROR! Unable to open camera\n";
    }

    // We get one frame from the camera to determine its size.
    cap.read(frame);
    flip(frame, frame, 0);
    if (frame.empty()) {
        cerr << "Error: couldn't capture an initial frame from camera. Exiting.\n";
        cap.release();
        glfwTerminate();
        return -1;
    }
    float videoAspectRatio = (float)frame.cols / (float)frame.rows;

    // --- Initialize OpenGL context (GLFW & GLAD - already complete) ---
    if (!initWindow("OpenCV to OpenGL Exercise", videoAspectRatio)) return -1;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Basic OpenGL setup
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glClearColor(0.1f, 0.1f, 0.2f, 0.0f);
    glEnable(GL_DEPTH_TEST);

    // Make our vertex array object
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Create shader map
    map<string, TextureShader*> shaders;

    addShader("none", "videoTextureShader.frag", shaders);
    addShader("greyscale", "greyscaleShader.frag", shaders);
    addShader("pixelated", "pixelatedShader.frag", shaders);
    addShader("blur", "blurShader.frag", shaders);
    addShader("sincity", "sinCityShader.frag", shaders);

    TextureShader* currentShader = shaders["none"];

    Scene* myScene = new Scene();

    // Pass aspectRatio to all shaders
    for (auto& pair : shaders) {
        GLuint programID = pair.second->getProgramID();
        GLuint ratioLoc = glGetUniformLocation(programID, "aspectRatio");
        glUseProgram(programID);
        glUniform1f(ratioLoc, videoAspectRatio);
    }

	// Make quad fill the screen
    glm::mat4 orthoProjection = glm::ortho(
        -videoAspectRatio, videoAspectRatio,
        -1.0f, 1.0f,
        0.1f, 100.0f
    );

    glm::mat4 viewMatrix = glm::lookAt(
        glm::vec3(0, 0, 5.0f),  // position
        glm::vec3(0, 0, 0.0f),  // look at center
        glm::vec3(0, 1, 0.0f)   // up vector
    );

    Camera* renderingCamera = new Camera(orthoProjection, viewMatrix);

    // Calculate aspect ratio and create a quad with the correct dimensions.
    Quad* myQuad = new Quad(videoAspectRatio);
    myQuad->setShader(currentShader);
    myScene->addObject(myQuad);

    // OpenGL texture object
    Texture* videoTexture = nullptr;

    // --- CREATE THE TEXTURE ---
    videoTexture = new Texture(frame.data,
        frame.cols,
        frame.rows,
        true);

	// Create Frame Buffer Object (FBO) for offscreen rendering
	// Same size as the video frame
    GLuint fboTexture;
    GLuint fbo = createFBO(BASE_WIDTH * videoAspectRatio, BASE_WIDTH, fboTexture);

    if (fbo == 0) {
        std::cerr << "Failed to create FBO!" << std::endl;
    }

    Texture* fboTextureObj = new Texture();
    fboTextureObj->m_textureID = fboTexture;


    // Create small FBO for pixelation filter
    GLuint fboSmallTexture;
    GLuint fboSmall = createFBO(int((BASE_WIDTH * videoAspectRatio) / 10), int((BASE_WIDTH) / 10), fboSmallTexture);

    if (fboSmall == 0) {
        std::cerr << "Failed to create FBO!" << std::endl;
    }

    Texture* fboSmallTextureObj = new Texture();
    fboSmallTextureObj->m_textureID = fboSmallTexture;

    currentShader->setTexture(videoTexture);

    int mode = -1;
    int renderMode = 0;
    bool hasChanged = 0;
    bool resolutionChanged = false;

	FrameStats stats = FrameStats();
    InputState input = InputState();

	// Save mouse wheel input (Scaling)
    glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) {
        InputState* input = static_cast<InputState*>(glfwGetWindowUserPointer(w));
        if (input) {
            input->mouse.scroll = static_cast<int>(yoffset);
        }
        });
    glfwSetWindowUserPointer(window, &input);

    GLuint programID;
    GLuint ratioLoc;
    bool isMultiPass = false;

    // Affine transformation
    float prevFrameRotation = 0.0f;
    float frameRotation = prevFrameRotation;
	float frameTranslationX = 0.0f;
	float frameTranslationY = 0.0f;
	float frameScale = 1.0f;
    while (!glfwWindowShouldClose(window)) {
        auto toBeTimed = [&]() {
			// Clear the screen, get new frame
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cap.read(frame);

            // Keyboard input for changing filters, etc.
            controlApp(mode, renderMode, resolutionChanged, stats, input);

            // Mouse input
            handleAffineInput(prevFrameRotation, frameRotation, frameTranslationX, frameTranslationY, frameScale, input);
            bool needsTransform = false;
            Mat transformationMatrix = getTransformationMatrix(frame, frameRotation, frameTranslationX,
                frameTranslationY, frameScale, needsTransform);

            if (needsTransform && !renderMode) {
                frame = TransformImageBackwards(frame, transformationMatrix);
            }

            if (resolutionChanged) {
                // Resize window
                glfwSetWindowSize(window, BASE_WIDTH * videoAspectRatio, BASE_WIDTH);
                glViewport(0, 0, BASE_WIDTH * videoAspectRatio, BASE_WIDTH);

                // Resize FBOs
                resizeFBO(fbo, fboTexture, BASE_WIDTH * videoAspectRatio, BASE_WIDTH);
                fboTextureObj->m_textureID = fboTexture;

                resizeFBO(fboSmall, fboSmallTexture,
                    int((BASE_WIDTH * videoAspectRatio) / 10),
                    int(BASE_WIDTH / 10));
                fboSmallTextureObj->m_textureID = fboSmallTexture;

                resolutionChanged = false;
            }

            bool shaderRendering = renderMode;
            isMultiPass = false;
            if (shaderRendering) {
                // Shader affine transformation application
                glm::mat4 model = getModelMatrix(frameTranslationX, frameTranslationY, frameRotation, frameScale, BASE_WIDTH, videoAspectRatio);
                myQuad->addTransform(model);

                // Shader filters
                applyShaderFilters(mode, shaders, myQuad, renderingCamera,
                    videoTexture, fboSmallTextureObj, fboSmall,
					BASE_WIDTH, videoAspectRatio);
            }
            else {
                myQuad->addTransform(glm::mat4(1.0f)); // Reset to identity
                if (input.filterChanged && input.renderChanged == 0) {
                    applyShader("none", currentShader, shaders, myQuad, videoTexture);
                }
                applyCVFilter(mode, frame);
            }

            if (!frame.empty() && videoTexture != nullptr) {
                flip(frame, frame, 0);
                videoTexture->update(frame.data, frame.cols, frame.rows, true);
            }

            // Keep passing videoAspectRatio uniform
            GLuint programID = currentShader->getProgramID();
            if (programID != 0) {
                GLuint ratioLoc = glGetUniformLocation(programID, "aspectRatio");
                glUseProgram(programID);
                glUniform1f(ratioLoc, videoAspectRatio);
            }

            if (!isMultiPass) {
                myScene->render(renderingCamera);
            }
            glfwSwapBuffers(window);
            glfwPollEvents();
            };

        double frameTime = timeFunction(toBeTimed);
        stats.totalTime += frameTime;
        stats.totalFrames++;

        double averageFrameTime = stats.totalTime / stats.totalFrames;
        double averageFPS = 1000.0 / averageFrameTime;

        if (stats.totalFrames % 60 == 0) {
            cout << "Average FPS: " << averageFPS
                << " (Avg frame time: " << averageFrameTime << " ms)" << endl;
        }

        if (waitKey(1) == 113) {
            break;
        }
    }

    // --- Cleanup -----------------------------------------------------------
    std::cout << "Closing application..." << endl;
    cap.release();
    delete myScene;
    delete renderingCamera;

    delete videoTexture;

    glDeleteFramebuffers(1, &fbo);
    delete fboTextureObj;

    glfwTerminate();
    return 0;
}