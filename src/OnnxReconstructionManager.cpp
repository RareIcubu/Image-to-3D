#include "OnnxReconstructionManager.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <fstream>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <algorithm>

OnnxReconstructionManager::OnnxReconstructionManager(QObject *parent) : IReconstructionManager(parent) {
    m_modelLoaded = false;
    m_ortEnv = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ImageTo3D_AI");
}

OnnxReconstructionManager::~OnnxReconstructionManager() {}

void OnnxReconstructionManager::cancel() {
    m_stopRequested = true;
}

void OnnxReconstructionManager::setModelPath(const QString &path) {
    m_targetModelPath = path;
}

bool OnnxReconstructionManager::loadModel(const QString &modelPath) {
    if (m_modelLoaded && m_currentModelPath == modelPath) return true;

    if (!QFile::exists(modelPath)) {
        emit errorOccurred("Brak pliku modelu: " + modelPath);
        return false;
    }

    try {
        emit progressUpdated("Ładowanie modelu: " + QFileInfo(modelPath).fileName(), 1);

        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        m_session = std::make_shared<Ort::Session>(*m_ortEnv, modelPath.toStdString().c_str(), sessionOptions);

        Ort::AllocatorWithDefaultOptions allocator;

        m_inputNameStrings.clear();
        m_outputNameStrings.clear();
        m_inputNames.clear();
        m_outputNames.clear();

        auto inName = m_session->GetInputNameAllocated(0, allocator);
        m_inputNameStrings.push_back(inName.get());
        m_inputNames.push_back(m_inputNameStrings.back().c_str());

        auto outName = m_session->GetOutputNameAllocated(0, allocator);
        m_outputNameStrings.push_back(outName.get());
        m_outputNames.push_back(m_outputNameStrings.back().c_str());

        auto typeInfo = m_session->GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        auto dims = tensorInfo.GetShape();

        if (dims.size() >= 4) {
            m_inputHeight = (dims[2] > 0) ? dims[2] : 384;
            m_inputWidth  = (dims[3] > 0) ? dims[3] : 384;
            qDebug() << "ORT: Wykryto rozdzielczość:" << m_inputWidth << "x" << m_inputHeight;
        }

        QString fileName = QFileInfo(modelPath).fileName().toLower();
        if (fileName.contains("dpt")) {
            m_modelType = DPT;
        } else if (fileName.contains("midas")) {
            m_modelType = MiDaS;
        } else {
            m_modelType = UNKNOWN;
        }

        m_modelLoaded = true;
        m_currentModelPath = modelPath;
        emit progressUpdated(QString("Model gotowy (%1x%2).").arg(m_inputWidth).arg(m_inputHeight), 5);
        return true;

    } catch (const Ort::Exception &e) {
        emit errorOccurred(QString("Błąd ONNX Runtime: %1").arg(e.what()));
        m_modelLoaded = false;
        return false;
    }
}

cv::Mat OnnxReconstructionManager::runInference(const cv::Mat &img) {
    if (!m_modelLoaded) return cv::Mat();

    cv::Mat imgRGB, imgResized;
    cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);
    cv::resize(imgRGB, imgResized, cv::Size(m_inputWidth, m_inputHeight), 0, 0, cv::INTER_CUBIC);

    imgResized.convertTo(imgResized, CV_32F, 1.0f / 255.0f);

    cv::Scalar mean(0.485, 0.456, 0.406);
    cv::Scalar std(0.229, 0.224, 0.225);
    imgResized = (imgResized - mean) / std;

    cv::Mat blob = cv::dnn::blobFromImage(imgResized);

    size_t inputTensorSize = 1 * 3 * m_inputHeight * m_inputWidth;
    std::vector<int64_t> inputDims = {1, 3, m_inputHeight, m_inputWidth};

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, blob.ptr<float>(), inputTensorSize, inputDims.data(), inputDims.size()
    );

    try {
        auto outputTensors = m_session->Run(
            Ort::RunOptions{nullptr}, m_inputNames.data(), &inputTensor, 1, m_outputNames.data(), 1
        );

        float* floatArr = outputTensors.front().GetTensorMutableData<float>();
        cv::Mat depthMap(m_inputHeight, m_inputWidth, CV_32F, floatArr);

        double minVal, maxVal;
        cv::minMaxLoc(depthMap, &minVal, &maxVal);
        
        cv::Mat depthNormalized;
        if (maxVal - minVal > 1e-6) {
            depthMap.convertTo(depthNormalized, CV_32F, 1.0f / (maxVal - minVal), -minVal / (maxVal - minVal));
        } else {
            depthNormalized = cv::Mat::zeros(depthMap.size(), CV_32F);
        }

        cv::Mat depthFinal;
        cv::resize(depthNormalized, depthFinal, img.size(), 0, 0, cv::INTER_CUBIC);

        return depthFinal.clone();

    } catch (const std::exception& e) {
        qCritical() << "Inference error:" << e.what();
        return cv::Mat();
    } catch (...) {
        qCritical() << "Unknown inference error";
        return cv::Mat();
    }
}

bool OnnxReconstructionManager::saveDepthAsPNG(const QString &path, const cv::Mat &depth32f) {
    cv::Mat depth8u;
    depth32f.convertTo(depth8u, CV_8U, 255.0);
    return cv::imwrite(path.toStdString(), depth8u);
}

bool OnnxReconstructionManager::savePointCloudPLY(const QString &path, const std::vector<Point3D> &points)
{
    std::ofstream ofs(path.toStdString(), std::ios::binary);
    if (!ofs.is_open()) return false;

    ofs << "ply\n";
    ofs << "format ascii 1.0\n";
    ofs << "element vertex " << points.size() << "\n";
    ofs << "property float x\n";
    ofs << "property float y\n";
    ofs << "property float z\n";
    ofs << "property uchar red\n";
    ofs << "property uchar green\n";
    ofs << "property uchar blue\n";
    ofs << "end_header\n";

    for (const auto &p : points) {
        ofs << p.x << " " << p.y << " " << p.z << " "
            << (int)p.r << " " << (int)p.g << " " << (int)p.b << "\n";
    }
    ofs.close();
    return true;
}

void OnnxReconstructionManager::analyzeDepthMap(const cv::Mat& depth) {
    double minVal, maxVal;
    cv::minMaxLoc(depth, &minVal, &maxVal);
    cv::Scalar mean, stddev;
    cv::meanStdDev(depth, mean, stddev);
    qDebug() << "Depth Stats -> Min:" << minVal << "Max:" << maxVal << "Mean:" << mean[0];
}

void OnnxReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputPath)
{
    m_timer.restart();
    
    if (!QDir().mkpath(outputPath)) {
        emit errorOccurred("Could not create output directory: " + outputPath);
        return;
    }

    emit progressUpdated("Workspace: " + outputPath, 0);
    
    if (m_targetModelPath.isEmpty()) {
        emit errorOccurred("No model selected for AI reconstruction.");
        return;
    }

    if (!loadModel(m_targetModelPath)) return;

    QDir imgDir(imagesPath);
    imgDir.setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    QFileInfoList files = imgDir.entryInfoList(QDir::Files);
    
    if (files.isEmpty()) { emit errorOccurred("Brak zdjęć w folderze: " + imagesPath); return; }

    int total = files.size();
    m_stopRequested = false;

    for (int i = 0; i < total; ++i) {
        if (m_stopRequested) {
            emit errorOccurred("Proces AI anulowany.");
            return;
        }

        QFileInfo fi = files[i];
        QString msg = QString("AI [%1/%2]: %3").arg(i+1).arg(total).arg(fi.fileName());
        emit progressUpdated(msg, -1);
        qDebug() << qPrintable(msg);

        cv::Mat img = cv::imread(fi.absoluteFilePath().toStdString());
        if (img.empty()) continue;

        cv::Mat imgRGB;
        cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);

        cv::Mat depth = runInference(img);
        if (depth.empty()) continue;
        
        analyzeDepthMap(depth);
        saveDepthAsPNG(outputPath + "/" + fi.completeBaseName() + "_depth.png", depth);

        std::vector<Point3D> currentPoints;

        float centerX = depth.cols / 2.0f;
        float centerY = depth.rows / 2.0f;
        float focalLength = 1000.0f; 
        float depthScale = 2000.0f; 

        for (int y = 0; y < depth.rows; y += m_subsample) {
            for (int x = 0; x < depth.cols; x += m_subsample) {
                float d = depth.at<float>(y, x);
                if (d < 0.1f) continue; 
                float Z = (1.0f / (d + 0.01f)) * depthScale;
                if (Z > 5000.0f) Z = 5000.0f;

                float X = (x - centerX) * Z / focalLength;
                float Y = (y - centerY) * Z / focalLength;

                Point3D p;
                p.x = X; p.y = Y; p.z = Z;
                cv::Vec3b color = imgRGB.at<cv::Vec3b>(y, x);
                p.r = color[0]; p.g = color[1]; p.b = color[2];

                currentPoints.emplace_back(p);
            }
        }

        QString plyPath = outputPath + "/" + fi.completeBaseName() + ".ply";
        savePointCloudPLY(plyPath, currentPoints);

        int percent = ((i + 1) * 100) / total;
        emit progressUpdated("", percent);
    }

    emit finished(outputPath);
    qDebug() << "AI Finished.";
}