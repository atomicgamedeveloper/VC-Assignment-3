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

int BASE_WIDTH = 720;

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
    // Posterize
    int divisor = 256 / 4;
    frame = (frame / divisor) * divisor;

    // Extract all channels
    Mat channels[3];
    split(frame, channels);
    Mat blue = channels[0];
    Mat green = channels[1];
    Mat red = channels[2];

    // Red mask: red is high AND green/blue are low
    Mat redMask = (red > 100) & (green < 50) & (blue < 50);

    // Black and white
    cvtColor(frame, frame, COLOR_BGR2GRAY);
    frame = (frame > 90) * 255;
    cvtColor(frame, frame, COLOR_GRAY2BGR);

    // Apply red only where it's actually red, not white
    frame.setTo(Scalar(0, 0, 255), redMask);

    return frame;
}

Mat filterSinCity2(Mat frame) {
    // Outline
    Mat canny = filterCanny(frame);

    // Keep reds
    Mat saturatedRed;
    extractChannel(frame, saturatedRed, 2);
    cvtColor(frame, frame, COLOR_BGR2GRAY);
    cvtColor(frame, frame, COLOR_GRAY2BGR);
    insertChannel(saturatedRed, frame, 2);

    // Outline
    Mat edges;
    extractChannel(canny, edges, 0);
    frame.setTo(Scalar(0, 0, 0), edges);
    return frame;
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

void controlApp(int& mode, int& renderMode, bool& hasChanged,
    double& totalTime, int& totalFrames, bool& resolutionChanged) {
    vector<string> filterLabels{ "None", "Grey", "Pixelated", "SinCity", "Median Blur","Gaussian" };
    vector<string> renderLabels{ "OpenCV","GLSL" };

    const int MAX_MODE = 3, MIN_MODE = -1;
    const int MAX_RENDER_MODE = 1, MIN_RENDER_MODE = 0;
    
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int mbleft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (mbleft == GLFW_PRESS) {
        // left button is pressed
    }

	std::cout << "\rCursor Position: (" << xpos << ", " << ypos << ")   " << std::flush;

    // Filters
    bool left = (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS);
    bool right = (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS);

    // Render mode
    bool up = (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS);
    bool down = (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS);

    // Resolution
    bool one = (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS);
    bool two = (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS);
    bool three = (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS);
    bool four = (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS);

    if (!hasChanged) {
        if (right && mode < MAX_MODE) mode++;
        if (left && mode > MIN_MODE) mode--;
        if (up && renderMode < MAX_RENDER_MODE) renderMode++;
        if (down && renderMode > MIN_RENDER_MODE) renderMode--;
		string input_detection = "\nInput detected.\n";

		resolutionChanged = one || two || three || four;
        if (one) {
            BASE_WIDTH = 480;
            std::cout << input_detection;
            std::cout << "\nResolution set to 480p.\n" << endl;
            resetAverage(totalTime, totalFrames);
		}
        if (two) {
            BASE_WIDTH = 720;
            std::cout << input_detection;
            std::cout << "\nResolution set to 720p.\n" << endl;
            resetAverage(totalTime, totalFrames);
		}
        if (three) {
            BASE_WIDTH = 1080;
            std::cout << input_detection;
            std::cout << "\nResolution set to 1080p.\n" << endl;
            resetAverage(totalTime, totalFrames);
        }
        if (four) {
            BASE_WIDTH = 1600;
            std::cout << input_detection;
            std::cout << "\nResolution set to 1600p.\n" << endl;
            resetAverage(totalTime, totalFrames);
        }

        if (right || left || up || down) {
            hasChanged = true;
            std::cout << input_detection;
            std::cout << "filter: " << filterLabels.at(mode + 1) << endl;
            std::cout << "render mode: " << renderLabels.at(renderMode) << "\n" << endl;
            resetAverage(totalTime, totalFrames);
        }
    }

    if (!left && !right && !up && !down &&
        !one && !two && !three && !four) hasChanged = false;
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

// Helper function to create an FBO with a color attachment texture
GLuint createFBO(int width, int height, GLuint& outTexture) {
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture to render to
    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Attach texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexture, 0);

    // Check FBO is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return 0; // Return 0 to indicate failure
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void resizeFBO(GLuint& fbo, GLuint& texture, int newWidth, int newHeight) {
    // Delete old resources
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &fbo);

    // Create new FBO with new size
    fbo = createFBO(newWidth, newHeight, texture);

    if (fbo == 0) {
        std::cerr << "Failed to recreate FBO!" << std::endl;
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
	string vertexShaderName = "videoTextureShader.vert";
    shaders["none"] = new TextureShader(vertexShaderName, "videoTextureShader.frag");
    shaders["greyscale"] = new TextureShader(vertexShaderName, "greyscaleShader.frag");
    shaders["pixelated"] = new TextureShader(vertexShaderName, "pixelatedShader.frag");
    shaders["blur"] = new TextureShader(vertexShaderName, "blurShader.frag");
    shaders["sincity"] = new TextureShader(vertexShaderName, "sinCityShader.frag");

    cout << "Texture shader program ID: " << shaders["none"]->getProgramID() << endl;
    cout << "Greyscale shader program ID: " << shaders["greyscale"]->getProgramID() << endl;
    cout << "Pixelated shader program ID: " << shaders["pixelated"]->getProgramID() << endl;
    cout << "Blur shader program ID: " << shaders["blur"]->getProgramID() << endl;
    cout << "SinCity shader program ID: " << shaders["sincity"]->getProgramID() << endl;


    if (shaders["none"]->getProgramID() == 0) {
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
    if (shaders["sincity"]->getProgramID() == 0) {
        cerr << "ERROR: SinCity shader failed to compile!" << endl;
    }

    TextureShader* currentShader = shaders["none"];

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
    GLuint fboTexture;
    GLuint fbo = createFBO(BASE_WIDTH * videoAspectRatio, BASE_WIDTH, fboTexture);

    if (fbo == 0) {
        std::cerr << "Failed to create FBO!" << std::endl;
        // Handle error appropriately
    }

    // Wrap the FBO texture so you can use it in your shaders
    Texture* fboTextureObj = new Texture();
    fboTextureObj->m_textureID = fboTexture;


    // Create small FBO for pixelation filter
    GLuint fboSmallTexture;
    GLuint fboSmall = createFBO(int((BASE_WIDTH * videoAspectRatio) / 10), int((BASE_WIDTH) / 10), fboSmallTexture);

    if (fboSmall == 0) {
        std::cerr << "Failed to create FBO!" << std::endl;
    }

    // Wrap the FBO texture so you can use it in your shaders
    Texture* fboSmallTextureObj = new Texture();
    fboSmallTextureObj->m_textureID = fboSmallTexture;


    currentShader->setTexture(videoTexture);

    cout << "Start grabbing" << endl
        << "Press any key to terminate" << endl;

    int mode = -1;
    int renderMode = 0;
    bool hasChanged = 0;
    bool resolutionChanged = false;
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

            controlApp(mode, renderMode, hasChanged, totalTime, totalFrames, resolutionChanged);

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
                switch (mode)
                {
                case -1: // None
                    currentShader = shaders["none"];
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

                    // PASS 1: Render to small FBO
                    glBindFramebuffer(GL_FRAMEBUFFER, fboSmall);
                    glViewport(0, 0, int((BASE_WIDTH * videoAspectRatio) / 10), int(BASE_WIDTH / 10));
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    myQuad->setShader(shaders["none"]);
                    shaders["none"]->setTexture(videoTexture);
                    myQuad->render(renderingCamera);

                    //// PASS 2: Upscale
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(0, 0, BASE_WIDTH * videoAspectRatio, BASE_WIDTH);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                    myQuad->setShader(shaders["none"]);
                    shaders["none"]->setTexture(fboSmallTextureObj);
                    myQuad->render(renderingCamera);

                    currentShader = shaders["none"];
                    break;
                case 2: // SinCity shader
                    currentShader = shaders["sincity"];
                    myQuad->setShader(currentShader);
                    currentShader->setTexture(videoTexture);
                    break;
                }
            }
            else {
                // Reset shader once
                if (hasChanged && renderMode == 0) {
                    currentShader = shaders["none"];
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
    //for (auto& pair : shaders) {
    //    delete pair;
    //}

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
    window = glfwCreateWindow(BASE_WIDTH * aspectRatio, BASE_WIDTH, windowName.c_str(), NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    return true;
}