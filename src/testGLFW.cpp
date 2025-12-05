#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <filesystem>
#include <opencv2/objdetect/aruco_board.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/charuco_detector.hpp>
#include <opencv2/aruco.hpp>

using namespace std;
using namespace cv;

unsigned int window_width = 800;
unsigned int window_height = 800;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const std::string calibrationFile = "C:/Users/Maloik/source/repos/VC-Assignment-3/src/cameraMatrix.yaml";

tuple<Mat, Mat> getCalibration() {
	const std::string path = calibrationFile;
	if (!std::filesystem::exists(path)) {
		std::cerr << "Camera calibration file not found: " << path << std::endl;
		return tuple(Mat::ones(Size(4,4),1), Mat::ones(Size(4, 4), 1));
	}

	cv::FileStorage fs(path, cv::FileStorage::READ);
	if (!fs.isOpened()) {
		std::cerr << "Failed to open calibration file: " << path << std::endl;
		return tuple(Mat::ones(Size(4, 4), 1), Mat::ones(Size(4, 4), 1));
	}

	cv::Mat cameraMatrix, distortionCoefficients;
	fs["camera_matrix"] >> cameraMatrix;
	fs["distortion_coefficients"] >> distortionCoefficients;
	//fs["rvecs"] >> rvecs;
	//fs["tvecs"] >> tvecs;
	fs.release();

	return tuple(cameraMatrix, distortionCoefficients);
}

int main() {
	if (!glfwInit()) { // Check that glfw works
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Create a window
	GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Hello World!", NULL, NULL);

	if (!window) {
		std::cout << "Failed to create the window!" << std::endl;
		glfwTerminate(); // Free resources
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to load opengl function pointers!" << std::endl;
		glfwTerminate();
		return -1;
	}

	const char* vertexShaderSrc =
		"#version 330 core\n"
		"layout (location = 0) in vec3 aPos;\n" // Specifying we have an input of vec3 called aPos
		"layout (location = 1) in vec3 aColor;\n"
		"layout (location = 2) in vec2 aTex;\n"
		"uniform mat4 model;\n" // Import matrices into vertex shader
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"uniform float scale;\n" // Vertex scale

		"out vec3 color;\n" // Output colour for fragment shader
		"out vec2 texCoord;\n" // Output texture coordinates for fragment shader
		// found at location 0
		"void main() {\n"
		"    gl_Position = projection * view * model * vec4(aPos * scale, 1.0f);\n" // Input pos passed to gl_Position
		"    color = aColor;\n" // vertex data colors -> color
		"    texCoord = aTex;\n" // vertex data texture coordinates -> texCoord
		"}\0"; //GLSL language, like OpenGL

	const char* fragmentShaderSrc =
		"#version 330 core\n"
		"out vec4 fragColor;\n" // Output, all needed to draw a single pixel
		"in vec3 color;\n"
		"in vec2 texCoord;\n"
		"uniform sampler2D tex0;\n" // Which texture unit for OpenGL to use
		"void main() {\n"
		"    fragColor = texture(tex0,texCoord);\n" //RGBA output
		"}\0";

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER); // Create empty vertex shader
	// Attach source to shader
	glShaderSource(vertexShader, 1, &vertexShaderSrc, 0); //id, 1 shader, source code pointer, 0
	glCompileShader(vertexShader);

	// Check success of compiling shader
	int success;
	char infoLog[512];

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertexShader, 512, 0, infoLog); // Try to get info log if it was not successful
		std::cout << "Failed to compile vertex shader! ERROR: " << infoLog << std::endl;
	}

	// Repeat for Fragment Shader

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSrc, 0);
	glCompileShader(fragmentShader);

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 512, 0, infoLog);
		std::cout << "Failed to compile fragment shader! ERROR: " << infoLog << std::endl;
	}

	// Create shader program with both shaders tied to it
	unsigned int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram,vertexShader);
	glAttachShader(shaderProgram,fragmentShader);
	// Combine
	glLinkProgram(shaderProgram);
	// Check success
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

	if (!success) {
		glGetProgramInfoLog(shaderProgram, 512, 0, infoLog);
		std::cout << "Failed to link shader program! ERROR: " << infoLog << std::endl;
	}

	// Inside shader program, free up resources
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// Draw square (normalized coordinates)
	float vertices[] = {
		// Front face
		-0.5f, -0.5f,  0.5f,    1.0f, 0.0f, 0.0f,    0.0f, 0.0f, // Bottom left
		 0.5f, -0.5f,  0.5f,    0.0f, 1.0f, 0.0f,    1.0f, 0.0f, // Top left
		 0.5f,  0.5f,  0.5f,    0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // Top right
		-0.5f,  0.5f,  0.5f,    1.0f, 1.0f, 1.0f,    0.0f, 1.0f, // Bottom right

		// Back face
		-0.5f, -0.5f, -0.5f,    1.0f, 0.0f, 0.0f,    1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,    0.0f, 0.0f, 1.0f,    0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,    1.0f, 1.0f, 1.0f,    1.0f, 1.0f
	};

	// Faces
	unsigned int indices[] = {
		// Front
		0, 1, 2,
		2, 3, 0,
		// Right
		1, 5, 6,
		6, 2, 1,
		// Back
		5, 4, 7,
		7, 6, 5,
		// Left
		4, 0, 3,
		3, 7, 4,
		// Top
		3, 2, 6,
		6, 7, 3,
		// Bottom
		4, 5, 1,
		1, 0, 4,
	};

	// Input to graphics pipeline - vertex shader
	// Creating memory on GPU
	unsigned int VAO, VBO, EBO; // create vertex buffer object and vertex array object (saving specifications instead of reinitializing again)
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO); // Bind it and specify type (array buffer)
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	// Fill memory with data
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,GL_STATIC_DRAW); // static = set only once, drawn a lot
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,GL_STATIC_DRAW);
	// gl_stream_draw = set once, used by gpu just a few times 
	// gl_dynamic_draw = data is changed a lot and drawn a lot

	// How to interpret memory
	// From vertex shader, location 0, size is 3 as vec3, float (opengl type),
	// no normalization, could be 0 and let open gl determine it or write it as per coordinate size,
	// start of the vertex - casted 0
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(3 * sizeof(float)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(6 * sizeof(float)));
	
	glEnableVertexAttribArray(0); // Enable vertex attrib pointer.
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	// Unbind everything.
	// Can be binded whenever needed through vertex array
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);



	// Draw camera plane.
	float quadVertices[] = {
		// Coords,               Colour,              Texture Coord
		-1.0f, -1.0f,  -1.0f,    1.0f, 0.0f, 0.0f,    0.0f, 0.0f, // Bottom left
		 1.0f, -1.0f,  -1.0f,    0.0f, 1.0f, 0.0f,    1.0f, 0.0f, // Top left
		 1.0f,  1.0f,  -1.0f,    0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // Top right
		-1.0f,  1.0f,  -1.0f,    1.0f, 1.0f, 1.0f,    0.0f, 1.0f  // Bottom right
	};

	unsigned int quadIndices[] = {
		1, 3, 0,
		1, 2, 3
	};

	unsigned int VAO_PLANE, VBO_PLANE, EBO_PLANE;
	glGenVertexArrays(1, &VAO_PLANE);
	glGenBuffers(1, &VBO_PLANE);
	glGenBuffers(1, &EBO_PLANE);
	glBindVertexArray(VAO_PLANE);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_PLANE);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_PLANE);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

	// Link VBO attributes like coordinates and colours to VAO.
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(3*sizeof(float)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(6 * sizeof(float)));

	// Enable attributes
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);


	const bool wireframeMode = false;
	if (wireframeMode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	GLuint uniID = glGetUniformLocation(shaderProgram, "scale"); 


	// Texture

	int imageWidth, imageHeight, channels;
	char* texturePath = "C:/Users/Maloik/source/repos/VC-Assignment-3/src/textures/pop_cat.png"; 
	stbi_set_flip_vertically_on_load(true);
	unsigned char* bytes = stbi_load(texturePath, &imageWidth, &imageHeight, &channels,0);

	GLuint tex0Uniform = glGetUniformLocation(shaderProgram, "tex0");

	GLuint texture;
	glGenTextures(1, &texture);

	// Make texture unit
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	// Adjust texture settings
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST); // Scale with nearest neighbour
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,GL_REPEAT); // Repeat image on x axis
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,GL_REPEAT); // Repeat image on y axis
		
	// Generate image - texture type, 0, colour channels, width, height, 0, colour channels, data type of pixels, data itself
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes); //RGBA for pngs, RGB for jpegs

	glGenerateMipmap(GL_TEXTURE_2D); // Generate smaller resolutions of the image for far distance

	// Free up resources
	stbi_image_free(bytes);
	glBindTexture(GL_TEXTURE_2D, 0);


	// Texture 2

	Mat frame;
	VideoCapture cap;

	int deviceID = 0;
	int apiID = CAP_ANY;
	cap.open(deviceID, apiID);

	if (!cap.isOpened()) {
		cerr << "ERROR! Unable to open camera\n";
		return -1;
	}

	// Get one frame from the camera to determine its size
	cap.read(frame);
	flip(frame, frame, 0);
	if (frame.empty()) {
		cerr << "Error: couldn't capture an initial frame from camera. Exiting.\n";
		cap.release();
		return -1;
	}
	float videoAspectRatio = (float)frame.cols / (float)frame.rows;

	//texturePath = "C:/Users/Maloik/source/repos/VC-Assignment-3/src/textures/evil_pop_cat.png";
	//stbi_set_flip_vertically_on_load(true);
	//bytes = stbi_load(texturePath, &imageWidth, &imageHeight, &channels, 0);

	GLuint texture_2;
	glGenTextures(1, &texture_2);

	// Make texture unit
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_2);

	// Adjust texture settings
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Scale with nearest neighbour
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repeat image on x axis
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Repeat image on y axis

	// Generate image - texture type, 0, colour channels, width, height, 0, colour channels, data type of pixels, data itself
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, frame.data); //RGBA for pngs, RGB for jpegs

	glGenerateMipmap(GL_TEXTURE_2D); // Generate smaller resolutions of the image for far distance

	// Free up resources
	glBindTexture(GL_TEXTURE_2D, 0);



	// Initialize detector
	int squareHorizontal = 5;
	int squareVertical = 7;
	float squareLength = 0.038f;
	float markerLength = 0.019f;
	int dictionaryId = cv::aruco::DICT_6X6_250;

	// Create CharucoBoard
	cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(dictionaryId);
	cv::aruco::CharucoBoard board(cv::Size(squareHorizontal, squareVertical), squareLength, markerLength, dictionary);

	// Create detectors
	cv::aruco::DetectorParameters detectorParams;
	cv::aruco::CharucoParameters charucoParams;
	cv::aruco::CharucoDetector charucoDetector(board, charucoParams, detectorParams);


	// Get calibration matrices
	auto [cameraMatrix, distortionCoefficients] = getCalibration();
	double fx = cameraMatrix.at<double>(0, 0); // Focal length in px
	double fy = cameraMatrix.at<double>(1, 1);
	double cx = cameraMatrix.at<double>(0, 2); // Principle point
	double cy = cameraMatrix.at<double>(1, 2);

	float rotation = 0.0f;
	double previousTime = glfwGetTime();

	cv::Mat rvec, tvec;
	std::vector<cv::Point3f> objectPoints;
	std::vector<cv::Point2f> imagePoints;

	Mat currentCharucoCorners, currentCharucoIds;
	vector<int> markerIds;
	vector<vector<Point2f>> markerCorners;
	vector<Point3f> currentObjectPoints;
	vector<Point2f> currentImagePoints;

	// For maintaining view of object on weak detection
	cv::Mat lastValidCharucoCorners, lastValidCharucoIds;
	cv::Mat lastValidRvec, lastValidTvec;
	bool poseHasBeenFoundOnce = false;

	double lastFrameTime = glfwGetTime();
	double startTime = lastFrameTime;
	double removeModelTimerMax = 2; // seconds
	double removeModelTimer = removeModelTimerMax;

	//glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(window)) {
		processInput(window);

		glClearColor(0.6f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderProgram);
		glUniform1i(tex0Uniform, 0);

		double currentTime = glfwGetTime();
		double deltaTime = currentTime - lastFrameTime;
		lastFrameTime = currentTime;

		cap.read(frame);

		cv::Mat undistorted;
		cv::undistort(frame, undistorted, cameraMatrix, distortionCoefficients);
		frame = undistorted;

		// Detect CharucoBoard
		currentCharucoCorners = cv::Mat();
		currentCharucoIds = cv::Mat();
		charucoDetector.detectBoard(frame, currentCharucoCorners, currentCharucoIds);

		bool poseIsValid = false;

		if (currentCharucoCorners.total() >= 6) {
			board.matchImagePoints(currentCharucoCorners, currentCharucoIds, objectPoints, imagePoints);

			if (objectPoints.size() >= 6) {
				poseIsValid = cv::solvePnP(objectPoints, imagePoints, cameraMatrix, distortionCoefficients, rvec, tvec);
				if (poseIsValid) {
					lastValidCharucoCorners = currentCharucoCorners.clone();
					lastValidCharucoIds = currentCharucoIds.clone();
					lastValidRvec = rvec.clone();
					lastValidTvec = tvec.clone();
					poseHasBeenFoundOnce = true;
					removeModelTimer = removeModelTimerMax;
				}
			}
		}

		// Fallback to previous position for lapses in detection
		if (!poseIsValid && poseHasBeenFoundOnce) {
			currentCharucoCorners = lastValidCharucoCorners.clone();
			currentCharucoIds = lastValidCharucoIds.clone();
			rvec = lastValidRvec.clone();
			tvec = lastValidTvec.clone();
			removeModelTimer -= deltaTime;
		}

		if (removeModelTimer <= 0) {
			poseHasBeenFoundOnce = false;
		}

		glm::mat4 viewAR(1.0f);
		if (poseIsValid || poseHasBeenFoundOnce) {
			// Draw debug visuals on frame
			cv::aruco::drawDetectedCornersCharuco(frame, currentCharucoCorners, currentCharucoIds);
			cv::drawFrameAxes(frame, cameraMatrix, distortionCoefficients, rvec, tvec, 0.1f);

			// Turn 3D rotationVector into 3x3 matrix
			Mat rotationMatrix;
			cv::Rodrigues(rvec, rotationMatrix);

			// Convert OpenCV coordinate system to OpenGL coordinate system
			cv::Mat tvecCopy = tvec.clone();
			cv::Mat rotCopy = rotationMatrix.clone();

			tvecCopy.at<double>(1) *= -1;
			tvecCopy.at<double>(2) *= -1;
			rotCopy.row(1) *= -1;
			rotCopy.row(2) *= -1;

			// Build view matrix
			cv::Mat newView = cv::Mat::zeros(4, 4, CV_64F);
			rotCopy.copyTo(newView(cv::Rect(0, 0, 3, 3)));
			newView.at<double>(3, 3) = 1.0;
			tvecCopy.copyTo(newView(cv::Rect(3, 0, 1, 3)));

			// Convert to glm
			for (int r = 0; r < 4; ++r) {
				for (int c = 0; c < 4; ++c) {
					viewAR[c][r] = (float)newView.at<double>(r, c);
				}
			}
		}

		if (!frame.empty()) {
			flip(frame, frame, 0);
			glBindTexture(GL_TEXTURE_2D, texture_2);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, frame.data);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glDisable(GL_DEPTH_TEST);

		int viewLoc = glGetUniformLocation(shaderProgram, "view");
		int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
		int modelLoc = glGetUniformLocation(shaderProgram, "model");

		glm::mat4 identityMat = glm::mat4(1.0f);
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(identityMat));
		glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(identityMat));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(identityMat));

		glUniform1f(uniID, 1.0f);
		glBindTexture(GL_TEXTURE_2D, texture_2);
		glBindVertexArray(VAO_PLANE);
		glDrawElements(GL_TRIANGLES, sizeof(quadIndices) / sizeof(int), GL_UNSIGNED_INT, 0);

		if (poseIsValid || poseHasBeenFoundOnce) {
			glEnable(GL_DEPTH_TEST);
			glClear(GL_DEPTH_BUFFER_BIT);

			// Create projection matrix
			double near = 0.01;
			double far = 10.0;

			glm::mat4 projectionAR = glm::mat4(0.0f);
			projectionAR[0][0] = 2.0f * fx / frame.cols;
			projectionAR[1][1] = 2.0f * fy / frame.rows;
			projectionAR[2][0] = 1.0f - 2.0f * cx / frame.cols;
			projectionAR[2][1] = -1.0f + (2.0f * cy + 2.0f) / frame.rows;
			projectionAR[2][2] = (near + far) / (near - far);
			projectionAR[2][3] = -1.0f;
			projectionAR[3][2] = 2.0f * near * far / (near - far);

			glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewAR));
			glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionAR));

			glm::mat4 cubeModel = glm::mat4(1.0f);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cubeModel));

			glUniform1f(uniID, 0.05f);
			glBindTexture(GL_TEXTURE_2D, texture);
			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(int), GL_UNSIGNED_INT, 0);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Free resources
	glDeleteProgram(shaderProgram);
	glDeleteBuffers(1, &VBO);
	glDeleteVertexArrays(1, &VAO);
	glDeleteTextures(1, &texture);

	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, true);
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	std::cout << "window size is " << width << " x " << height << std::endl;
	window_width = width;
	window_height = height;
	glViewport(0, 0, window_width, window_height); // Resetting the rendering area (view port)
}
