#include <opencv2/opencv.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <iostream>
#include <vector>
#include <filesystem>

void testCamera(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Camera calibration file not found: " << path << std::endl;
        return;
    }

    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "Failed to open calibration file: " << path << std::endl;
        return;
    }

    cv::Mat cameraMatrix, distortionCoefficients;
    fs["camera_matrix"] >> cameraMatrix;
    fs["distortion_coefficients"] >> distortionCoefficients;
    fs.release();

    if (cameraMatrix.empty() || distortionCoefficients.empty()) {
        std::cerr << "Failed to read calibration data from file" << std::endl;
        return;
    }

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera" << std::endl;
        return;
    }

    cv::Mat frameBefore, frameAfter;
    cv::namedWindow("Before", cv::WINDOW_NORMAL);
    cv::namedWindow("After", cv::WINDOW_NORMAL);

    while (true) {
        cap >> frameBefore;
        if (frameBefore.empty()) break;

        cv::imshow("Before", frameBefore);
        cv::undistort(frameBefore, frameAfter, cameraMatrix, distortionCoefficients);
        cv::imshow("After", frameAfter);

        if (cv::waitKey(30) == 27) break;  // ESC to exit
    }
}

void calibrateCamera(std::string calibrationFile) {
    // CharucoBoard parameters
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

    std::vector<std::vector<cv::Point3f>> allObjectPoints;
    std::vector<std::vector<cv::Point2f>> allImagePoints;
    cv::Size imageSize;

    const std::string ImagePath = "C:/Users/Maloik/source/repos/VC-Assignment-3/src/images/";

    for (int i = 1; i <= 66; i++) {
        std::string filename = ImagePath + std::to_string(i) + ".jpg";
        cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);

        if (image.empty()) {
            std::cerr << "Failed to load: " << filename << std::endl;
            continue;
        }

        // Detect Charuco board
        std::vector<cv::Point2f> charucoCorners;
        std::vector<int> charucoIds;
        std::vector<std::vector<cv::Point2f>> markerCorners;
        std::vector<int> markerIds;

        charucoDetector.detectBoard(image, charucoCorners, charucoIds, markerCorners, markerIds);

        // Need at least some corners for calibration
        if (charucoIds.size() < 4) {
            std::cerr << "Not enough corners in: " << filename << std::endl;
            continue;
        }

        // Match to 3D object points
        std::vector<cv::Point3f> objectPoints;
        std::vector<cv::Point2f> imagePoints;
        board.matchImagePoints(charucoCorners, charucoIds, objectPoints, imagePoints);

        allObjectPoints.push_back(objectPoints);
        allImagePoints.push_back(imagePoints);
        imageSize = image.size();

        // Draw detections
        cv::aruco::drawDetectedMarkers(image, markerCorners, markerIds);
        cv::aruco::drawDetectedCornersCharuco(image, charucoCorners, charucoIds);
    }

    if (allObjectPoints.empty()) {
        std::cerr << "No valid calibration images found!" << std::endl;
        return;
    }

    // Calibration
    cv::Mat cameraMatrix, distortionCoefficients;
    std::vector<cv::Mat> rvecs, tvecs;

    double reprojectionError = cv::calibrateCamera(allObjectPoints, allImagePoints, imageSize,
        cameraMatrix, distortionCoefficients, rvecs, tvecs);

    std::cout << "\n=== Calibration Complete ===" << std::endl;
    std::cout << "Reprojection error: " << reprojectionError << std::endl;
    std::cout << "Camera matrix:\n" << cameraMatrix << std::endl;
    std::cout << "Distortion coefficients:\n" << distortionCoefficients << std::endl;

    // Save calibration to absolute path
    cv::FileStorage fs(calibrationFile, cv::FileStorage::WRITE);
    if (!fs.isOpened()) {
        std::cerr << "Failed to create calibration file: " << calibrationFile << std::endl;
        return;
    }
    fs << "camera_matrix" << cameraMatrix;
    fs << "distortion_coefficients" << distortionCoefficients;
    fs.release();

    std::cout << "Calibration saved to: " << calibrationFile << std::endl;
    return;
}

int main() {
    const std::string calibrationFile = "C:/Users/Maloik/source/repos/VC-Assignment-3/src/cameraMatrix.yaml";

    if (!std::filesystem::exists(calibrationFile)) {
        std::cerr << "Camera calibration file not found: " << calibrationFile << std::endl;
        calibrateCamera(calibrationFile);
    }

    testCamera(calibrationFile);

    return 0;
}