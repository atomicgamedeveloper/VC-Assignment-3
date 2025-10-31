#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>
#include <map>

using namespace std;
using namespace cv;

#include <common/Shader.hpp>
#include <common/Camera.hpp>
#include <common/Scene.hpp>
#include <common/Object.hpp>
#include <common/TextureShader.hpp>
#include <common/Quad.hpp>
#include <common/Texture.hpp>

GLFWwindow* window = nullptr;
int BASE_WIDTH = 720;
const char* window_name = "Live Camera Project";

bool initWindow(const std::string& windowName, float aspectRatio) {
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

cv::Mat TransformImageBackwards(const cv::Mat& img, const cv::Mat& M) {
    Mat invM = M.inv();
    Mat newImg = Mat::zeros(img.size(), img.type());
    for (int row = 0; row < img.rows; row++) {
        for (int col = 0; col < img.cols; col++) {
            Mat pos = (cv::Mat_<float>(3, 1) << col, row, 1);
            Mat srcPos = invM * pos;
            int r = (int)round(srcPos.at<float>(1, 0));
            int c = (int)round(srcPos.at<float>(0, 0));
            if (r < img.rows && c < img.cols && r >= 0 && c >= 0) {
                Vec3b srcPixel = img.at<cv::Vec3b>(r, c);
                newImg.at<cv::Vec3b>(row, col) = srcPixel;
            }
        }
    }
    return newImg;
}

double timeFunction(std::function<void()> codeBlock) {
    auto start = std::chrono::high_resolution_clock::now();
    codeBlock();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0;
}

GLuint createFBO(int width, int height, GLuint& outTexture) {
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return fbo;
}

void resizeFBO(GLuint& fbo, GLuint& texture, int newWidth, int newHeight) {
    glDeleteTextures(1, &texture);
    glDeleteFramebuffers(1, &fbo);
    fbo = createFBO(newWidth, newHeight, texture);
    if (fbo == 0) {
        std::cerr << "Failed to recreate FBO!" << std::endl;
    }
}

void addShader(const std::string& label, const std::string& fragmentShaderName,
    std::map<std::string, TextureShader*>& shaders) {
    string vertexShader = "videoTextureShader.vert";
    shaders[label] = new TextureShader(vertexShader, fragmentShaderName);
    cout << "Texture shader program ID: " << shaders[label]->getProgramID() << endl;
    if (shaders[label]->getProgramID() == 0) {
        cerr << "ERROR: Texture shader failed to compile!" << endl;
    }
}

cv::Mat getTranslationMatrix(float xoff, float yoff) {
    Mat trans = Mat::eye(3, 3, CV_32F);
    trans.at<float>(0, 2) = xoff;
    trans.at<float>(1, 2) = yoff;
    return trans;
}

cv::Mat getRotationMatrix(float angleDegrees, cv::Point2f center) {
    float angleRadians = angleDegrees * static_cast<float>(CV_PI) / 180.0f;
    Mat rot = Mat::eye(3, 3, CV_32F);
    rot.at<float>(0, 0) = cos(angleRadians);
    rot.at<float>(0, 1) = -sin(angleRadians);
    rot.at<float>(1, 0) = sin(angleRadians);
    rot.at<float>(1, 1) = cos(angleRadians);
    Mat translateToOrigin = getTranslationMatrix(-center.x, -center.y);
    Mat translateBack = getTranslationMatrix(center.x, center.y);
    return translateBack * rot * translateToOrigin;
}

cv::Mat getScaleMatrix(float xscale, float yscale, cv::Point2f center) {
    Mat scale = Mat::eye(3, 3, CV_32F);
    scale.at<float>(0, 0) = xscale;
    scale.at<float>(1, 1) = yscale;
    Mat translateToOrigin = getTranslationMatrix(-center.x, -center.y);
    Mat translateBack = getTranslationMatrix(center.x, center.y);
    return translateBack * scale * translateToOrigin;
}

void setShaderMVP(TextureShader* shader, const glm::mat4& MVP) {
    GLuint programID = shader->getProgramID();
    glUseProgram(programID);
    GLuint matrixID = glGetUniformLocation(programID, "MVP");
    glUniformMatrix4fv(matrixID, 1, GL_FALSE, &MVP[0][0]);
}

Mat getTransformationMatrix(Mat sourceMat, float rotation, float translationX, float translationY, float scale, bool& needsTransform) {
    Mat compositeTransform = Mat::eye(3, 3, CV_32F);
    needsTransform = false;

    Point2f center(sourceMat.cols / 2.0f + translationX,
        sourceMat.rows / 2.0f + translationY);

    if (fabs(scale - 1.0f) > 0.0001f) {
        Mat scaleMatrix = getScaleMatrix(scale, scale, center);
        compositeTransform = compositeTransform * scaleMatrix;
        needsTransform = true;
    }

    if (fabs(rotation) > 0.1f) {
        Mat rotationMatrix = getRotationMatrix(rotation, center);
        compositeTransform = compositeTransform * rotationMatrix;
        needsTransform = true;
    }

    if (fabs(translationX) > 0.1f || fabs(translationY) > 0.1f) {
        Mat translationMatrix = getTranslationMatrix(translationX, translationY);
        compositeTransform = compositeTransform * translationMatrix;
        needsTransform = true;
    }
    return compositeTransform;
}

void applyShader(const std::string& shaderName,
    TextureShader*& currentShader,
    std::map<std::string, TextureShader*>& shaders,
    Quad* quad,
    Texture* videoTexture) {
    currentShader = shaders[shaderName];
    quad->setShader(currentShader);
    currentShader->setTexture(videoTexture);
}

bool applyShaderFilters(int mode,
    map<string, TextureShader*>& shaders,
    Quad* quad,
    Camera* camera,
    Texture* videoTexture,
    Texture* fboSmallTexture,
    GLuint fboSmall,
    int baseWidth,
    float videoAspectRatio) {

    bool isMultiPass = false;
    TextureShader* currentShader = nullptr;

    switch (mode) {
    case -1: // None
        applyShader("none", currentShader, shaders, quad, videoTexture);
        break;

    case 0: // Grayscale
        applyShader("greyscale", currentShader, shaders, quad, videoTexture);
        break;

    case 1: // Pixelated (multi-pass)
        isMultiPass = true;
        {
            int smallWidth = int((baseWidth * videoAspectRatio) / 10);
            int smallHeight = int(baseWidth / 10);
            int fullWidth = baseWidth * videoAspectRatio;
            int fullHeight = baseWidth;

            // Pass 1: Downscale to small FBO
            glBindFramebuffer(GL_FRAMEBUFFER, fboSmall);
            glViewport(0, 0, smallWidth, smallHeight);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            quad->setShader(shaders["none"]);
            shaders["none"]->setTexture(videoTexture);
            quad->render(camera);

            // Pass 2: Upscale to screen
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fullWidth, fullHeight);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            quad->setShader(shaders["none"]);
            shaders["none"]->setTexture(fboSmallTexture);
            quad->render(camera);
        }
        break;

    case 2: // Sin City
        applyShader("sincity", currentShader, shaders, quad, videoTexture);
        break;

    default:
        applyShader("none", currentShader, shaders, quad, videoTexture);
        break;
    }

    // Update aspect ratio uniform for the current shader
    if (currentShader != nullptr) {
        GLuint programID = currentShader->getProgramID();
        if (programID != 0) {
            GLuint ratioLoc = glGetUniformLocation(programID, "aspectRatio");
            glUseProgram(programID);
            glUniform1f(ratioLoc, videoAspectRatio);
        }
    }

    return isMultiPass;
}

glm::mat4 getModelMatrix(float frameTranslationX,
    float frameTranslationY,
    float frameRotation,
    float frameScale,
    int   baseWidth,
    float videoAspectRatio)
{
    float normalizedX = (frameTranslationX / (baseWidth * videoAspectRatio)) * 2.0f * videoAspectRatio;
    float normalizedY = (frameTranslationY / baseWidth) * 2.0f;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(normalizedX, -normalizedY, 0.0f));
    model = glm::rotate(model, glm::radians(frameRotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(frameScale, frameScale, 1.0f));
    return model;
}