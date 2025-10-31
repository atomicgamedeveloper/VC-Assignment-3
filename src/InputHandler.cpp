#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../hpp/InputHandler.hpp"
#include "../hpp/AppUtilities.hpp"
#include <iostream>
using namespace std;

void FrameStats::reset() {
    totalTime = 0;
    totalFrames = 0;
}

void FrameStats::addFrame(double frameTime) {
    totalTime += frameTime;
    totalFrames++;
}

void handleMouseInput(MouseState& mouse) {
    glfwGetCursorPos(window, &mouse.xpos, &mouse.ypos);
    cout << "\rCursor Position: (" << mouse.xpos << ", " << mouse.ypos << ")   " << flush;

    mouse.mbleft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mouse.mbleft) {
        if (!mouse.prevLeft) {
            mouse.leftClickX = mouse.xpos;
			mouse.leftClickY = mouse.ypos;
            mouse.prevLeft = true;
            cout << "\nLeft mouse button is pressed at (" <<
                mouse.xpos << ", " << mouse.ypos << ")\n";
        }
    }
    else {
		mouse.prevLeft = false;
    }

    mouse.mbright = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (mouse.mbright) {
        if (!mouse.prevRight) {
            mouse.rightClickX = mouse.xpos;
            mouse.rightClickY = mouse.ypos;
            mouse.prevRight = true;
            cout << "\nRight mouse button is pressed at (" <<
                mouse.xpos << ", " << mouse.ypos << ")\n";
        }
    }
    else {
		mouse.prevRight = false;
    }
}

void handleFilterInput(int& mode, InputState& input, FrameStats& stats) {
    const int MAX_MODE = 3, MIN_MODE = -1;
    vector<string> filterLabels{ "None", "Grey", "Pixelated", "SinCity", "Median Blur", "Gaussian" };

    bool left = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;

    if ((left || right) && !input.filterChanged) {
        if (right && mode < MAX_MODE) mode++;
        if (left && mode > MIN_MODE) mode--;

        input.filterChanged = true;
        cout << "\nInput detected.\nfilter: " << filterLabels.at(mode + 1) << endl;
        stats.reset();
    }

    if (!left && !right)
        input.filterChanged = false;
}

void handleRenderModeInput(int& renderMode, InputState& input, FrameStats& stats) {
    const int MAX_RENDER_MODE = 1, MIN_RENDER_MODE = 0;
    vector<string> renderLabels{ "OpenCV", "GLSL" };

    bool up = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    bool down = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;

    if ((up || down) && !input.renderChanged) {
        if (up && renderMode < MAX_RENDER_MODE) renderMode++;
        if (down && renderMode > MIN_RENDER_MODE) renderMode--;

        input.renderChanged = true;
        cout << "\nInput detected.\nrender mode: " << renderLabels.at(renderMode) << "\n" << endl;
        stats.reset();
    }

    if (!up && !down)
        input.renderChanged = false;
}

void handleResolutionInput(bool& resolutionChanged, InputState& input, FrameStats& stats) {
    bool one = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool two = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool three = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    bool four = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;

    if ((one || two || three || four) && !input.resolutionChanged) {
        cout << "\nInput detected.\n";

        if (one) {
            BASE_WIDTH = 240;
        }
        else if (two) {
            BASE_WIDTH = 720;
        }
        else if (three) {
            BASE_WIDTH = 1080;
        }
        else if (four) {
            BASE_WIDTH = 1600;
        }

        cout << "Resolution set to " << BASE_WIDTH << "p.\n" << endl;
        input.resolutionChanged = true;
        resolutionChanged = true;
        stats.reset();
    }

    if (!one && !two && !three && !four)
        input.resolutionChanged = false;
}

void handleQuit() {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void handleAffineInput(float& prevFrameRotation, float& frameRotation,
    float& frameTranslationX, float& frameTranslationY,
    float& frameScale, InputState& input) {
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        frameRotation = 0;
        frameTranslationX = 0;
        frameTranslationY = 0;
        frameScale = 1.0f;
    }

    if (input.mouse.mbright) {
        double dx = input.mouse.xpos - input.mouse.rightClickX;
        double dy = input.mouse.ypos - input.mouse.rightClickY;
        double dragDistance = sqrt(dx * dx + dy * dy);
        frameRotation = static_cast<float>(dragDistance);
    }

    if (input.mouse.mbleft) {
        double dx = input.mouse.xpos - input.mouse.leftClickX;
        double dy = input.mouse.ypos - input.mouse.leftClickY;
        frameTranslationX = static_cast<float>(dx);
        frameTranslationY = static_cast<float>(dy);
    }

    if (input.mouse.scroll != 0) {
        frameScale *= std::pow(1.1f, static_cast<float>(input.mouse.scroll));
        frameScale = std::max(0.1f, frameScale);

        input.mouse.scroll = 0;
    }
}

void controlApp(int& mode, int& renderMode, bool& resolutionChanged, FrameStats& stats, InputState& input) {
    handleMouseInput(input.mouse);
    handleFilterInput(mode, input, stats);
    handleRenderModeInput(renderMode, input, stats);
    handleResolutionInput(resolutionChanged, input, stats);
    handleQuit();
}
