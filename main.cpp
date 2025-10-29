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

#include <glad/glad.h>
#define GLAD_GL_IMPLEMENTATION

#include <GLFW/glfw3.h>
GLFWwindow* window;

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
const char* window_name = "Live Camera Project";

int lowThreshold = 0;
const int max_lowThreshold = 100;
const int _ratio = 3;
const int kernel_size = 3;

// Helper function to initialize the window
bool initWindow(std::string windowName, float aspectRatio);

Mat filterPixelate(Mat frame) {
    Mat tinyFrame;
    Mat bigFrame;
    int pixelation = 5;
    resize(frame, tinyFrame, Size(frame.cols / pixelation, frame.rows / pixelation), 0, 0, INTER_NEAREST);
    resize(tinyFrame, bigFrame, Size(frame.cols, frame.rows), 0, 0, INTER_NEAREST);
    return bigFrame;
}

Mat filterCanny(Mat frame) {
    Mat temp;
    cvtColor(frame, temp, COLOR_BGR2GRAY);
    Canny(temp, temp, 60, 180);
    cvtColor(temp, temp, COLOR_GRAY2BGR);
    return temp;
}

Mat filterSinCity(Mat frame) {
    // Saturate in HSV colour space
    Mat hsv;
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    vector<Mat> channel;
    split(hsv, channel);

    channel[1] = min(channel[1] * 1.4, 255);

    merge(channel, hsv);
    Mat temp;
    cvtColor(hsv, temp, COLOR_HSV2BGR);

    // Outline
    Mat canny = filterCanny(temp);

    // Keep reds
    Mat saturatedRed;
    extractChannel(temp, saturatedRed, 2);
    cvtColor(temp, temp, COLOR_BGR2GRAY);
    cvtColor(temp, temp, COLOR_GRAY2BGR);
    insertChannel(saturatedRed, temp, 2);

    // Outline
    Mat edges;
    extractChannel(canny, edges, 0);
    temp.setTo(Scalar(0, 0, 0), edges);

    return temp;
}

double timeFunction(function<void()> codeBlock) {
    auto start = std::chrono::high_resolution_clock::now();

    codeBlock();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double milliseconds = duration.count() / 1000.0;
    return milliseconds;
}

void resetAverage(double& totalTime, int& totalFrames) {
    totalTime = 0;
    totalFrames = 0;
}

void controlFilters(int& mode, int& renderMode, bool& hasChanged, double& totalTime, int& totalFrames) {
    vector<string> filterLabels{ "None", "Grey", "Pixelated", "Canny", "Median Blur","Gaussian" };
    vector<string> renderLabels{ "OpenCV","GLSL" };

    const int MAX_MODE = 3, MIN_MODE = -1;
    const int MAX_RENDER_MODE = 1, MIN_RENDER_MODE = 0;

    bool left = GetAsyncKeyState(VK_LEFT) & 0x8000;
    bool right = GetAsyncKeyState(VK_RIGHT) & 0x8000;
    bool up = GetAsyncKeyState(VK_UP) & 0x8000;
    bool down = GetAsyncKeyState(VK_DOWN) & 0x8000;

    if (!hasChanged) {
        if (right && mode < MAX_MODE) mode++;
        if (left && mode > MIN_MODE) mode--;
        if (up && renderMode < MAX_RENDER_MODE) renderMode++;
        if (down && renderMode > MIN_RENDER_MODE) renderMode--;

        if (right || left || up || down) {
            hasChanged = true;
            std::cout << "\nInput detected." << endl;
            std::cout << "filter: " << filterLabels.at(mode + 1) << endl;
            std::cout << "render mode: " << renderLabels.at(renderMode) << "\n" << endl;
            resetAverage(totalTime, totalFrames);
        }
    }

    if (!left && !right && !up && !down) hasChanged = false;
}

void applyCVFilter(int& mode, Mat& frame) {
    if (mode == 0) {
        cvtColor(frame, frame, COLOR_BGR2GRAY);
        cvtColor(frame, frame, COLOR_GRAY2BGR);
    }
    else if (mode == 1) {
        frame = filterPixelate(frame);
    }
    else if (mode == 2) {
        frame = filterSinCity(frame);
    }
    else if (mode == 3) {
        medianBlur(frame, frame, 15);
    }
}

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
    cap.read(frame); // cap >> frame
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
    glClearColor(0.1f, 0.1f, 0.2f, 0.0f); // A dark blue background
    glEnable(GL_DEPTH_TEST);

    // Make our vertex array object
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Create shader map
    map<string, TextureShader*> shaders;
    shaders["texture"] = new TextureShader("videoTextureShader.vert", "videoTextureShader.frag");
    shaders["greyscale"] = new TextureShader("greyscaleShader.vert", "greyscaleShader.frag");
    shaders["pixelated"] = new TextureShader("pixelatedShader.vert", "pixelatedShader.frag");
    shaders["blur"] = new TextureShader("blurShader.vert", "blurShader.frag");

    cout << "Texture shader program ID: " << shaders["texture"]->getProgramID() << endl;
    cout << "Greyscale shader program ID: " << shaders["greyscale"]->getProgramID() << endl;
    cout << "Pixelated shader program ID: " << shaders["pixelated"]->getProgramID() << endl;
    cout << "Blur shader program ID: " << shaders["blur"]->getProgramID() << endl;

    if (shaders["texture"]->getProgramID() == 0) {
        cerr << "ERROR: Texture shader failed to compile!" << endl;
    }
    if (shaders["greyscale"]->getProgramID() == 0) {
        cerr << "ERROR: Greyscale shader failed to compile!" << endl;
    }
    if (shaders["pixelated"]->getProgramID() == 0) {
        cerr << "ERROR: Pixelated shader failed to compile!" << endl;
    }
    if (shaders["blur"]->getProgramID() == 0) {
        cerr << "ERROR: Blur shader failed to compile!" << endl;
    }

    TextureShader* currentShader = shaders["texture"];

    Scene* myScene = new Scene();

    // Pass aspectRatio to all shaders
    for (auto& pair : shaders) {
        GLuint programID = pair.second->getProgramID();
        GLuint ratioLoc = glGetUniformLocation(programID, "aspectRatio");
        glUseProgram(programID);
        glUniform1f(ratioLoc, videoAspectRatio);
    }

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

    // This variable will hold our OpenGL texture.
    Texture* videoTexture = nullptr;

    // --- CREATE THE TEXTURE ---
    videoTexture = new Texture(frame.data,
        frame.cols,
        frame.rows,
        true);

    // Create FBO
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture to render to
    GLuint fboTexture;
    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 720 * videoAspectRatio, 720, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Attach texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    // Check FBO is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Wrap the FBO texture so you can use it in your shaders
    Texture* fboTextureObj = new Texture();
    fboTextureObj->m_textureID = fboTexture;

    currentShader->setTexture(videoTexture);

    cout << "Start grabbing" << endl
        << "Press any key to terminate" << endl;

    int mode = -1;
    int renderMode = 0;
    bool hasChanged = 0;
    double totalTime = 0;
    int totalFrames = 0;

    GLuint programID;
    GLuint ratioLoc;
    bool isMultiPass = false;
    while (!glfwWindowShouldClose(window)) {
        auto toBeTimed = [&]() {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            cap.read(frame);

            controlFilters(mode, renderMode, hasChanged, totalTime, totalFrames);
            bool shaderRendering = renderMode;
            isMultiPass = false;
            if (shaderRendering) {
                switch (mode)
                {
                case -1: // None
                    currentShader = shaders["texture"];
                    myQuad->setShader(currentShader);
                    currentShader->setTexture(videoTexture);
                    break;
                case 0: // B&W
                    currentShader = shaders["greyscale"];
                    myQuad->setShader(currentShader);
                    currentShader->setTexture(videoTexture);
                    break;
                case 1: // Pixelated shader
                    isMultiPass = true;

                    // PASS 1: Render to FBO
                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    myQuad->setShader(shaders["pixelated"]);
                    shaders["pixelated"]->setTexture(videoTexture);
                    myQuad->render(renderingCamera);

                    // PASS 2: Render to screen using FBO texture
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


                    // Pass blur uniforms to blur shader
                    GLuint _programID = shaders["blur"]->getProgramID();
                    GLuint directionLoc = glGetUniformLocation(_programID, "direction");
                    glUseProgram(_programID);
                    vec2 dir(1, 0);
                    glUniform2fv(directionLoc, 1, &dir[0]);

                    GLuint resolutionLoc = glGetUniformLocation(_programID, "resolution");
                    glUseProgram(_programID);
                    vec2 res(720 * videoAspectRatio, 720);
                    glUniform2fv(resolutionLoc, 1, &res[0]);

                    myQuad->setShader(shaders["blur"]);
                    shaders["blur"]->setTexture(fboTextureObj);
                    myQuad->render(renderingCamera);
                    break;
                }
            }
            else {
                // Reset shader once
                if (hasChanged && renderMode == 0) {
                    currentShader = shaders["texture"];
                    myQuad->setShader(currentShader);
                    currentShader->setTexture(videoTexture);
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
        totalTime += frameTime;
        totalFrames++;

        double averageFrameTime = totalTime / totalFrames;
        double averageFPS = 1000.0 / averageFrameTime;

        if (totalFrames % 60 == 0) {
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

    // Clean up all shaders in map
    for (auto& pair : shaders) {
        delete pair.second;
    }

    delete videoTexture;

    glDeleteFramebuffers(1, &fbo);
    delete fboTextureObj;

    glfwTerminate();
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Helper: initWindow (GLFW)                                                 */
/* ------------------------------------------------------------------------- */
bool initWindow(std::string windowName, float aspectRatio) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(720 * aspectRatio, 720, windowName.c_str(), NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    return true;
}