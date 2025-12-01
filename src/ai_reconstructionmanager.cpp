#include "ai_reconstructionmanager.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <fstream>
#include <opencv2/dnn.hpp> 

AIReconstructionManager::AIReconstructionManager(QObject *parent) : QObject(parent) {
    m_modelLoaded = false;
    m_ortEnv = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ImageTo3D_AI");
}

AIReconstructionManager::~AIReconstructionManager() {}

bool AIReconstructionManager::loadModel(const QString &modelPath) {
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

        // Detekcja wejść/wyjść i rozmiaru
        Ort::AllocatorWithDefaultOptions allocator;
        
        m_inputNameStrings.clear();
        m_outputNameStrings.clear();
        m_inputNames.clear();
        m_outputNames.clear();

        // Wejście (zakładamy 1)
        auto inName = m_session->GetInputNameAllocated(0, allocator);
        m_inputNameStrings.push_back(inName.get());
        m_inputNames.push_back(m_inputNameStrings.back().c_str());

        // Wyjście (zakładamy 1)
        auto outName = m_session->GetOutputNameAllocated(0, allocator);
        m_outputNameStrings.push_back(outName.get());
        m_outputNames.push_back(m_outputNameStrings.back().c_str());

        // Rozmiar
        auto typeInfo = m_session->GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        auto dims = tensorInfo.GetShape();

        if (dims.size() >= 4) {
            // Jeśli dynamiczny (-1), ustaw domyślnie 384, w przeciwnym razie weź z modelu
            m_inputHeight = (dims[2] > 0) ? dims[2] : 384;
            m_inputWidth  = (dims[3] > 0) ? dims[3] : 384;
            qDebug() << "ORT: Wykryto rozdzielczość:" << m_inputWidth << "x" << m_inputHeight;
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

cv::Mat AIReconstructionManager::runInference(const cv::Mat &img) {
    if (!m_modelLoaded) return cv::Mat();

    // Preprocessing
    cv::Mat imgRGB, imgResized;
    cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);
    cv::resize(imgRGB, imgResized, cv::Size(m_inputWidth, m_inputHeight), 0, 0, cv::INTER_CUBIC);
    
    imgResized.convertTo(imgResized, CV_32F, 1.0f / 255.0f);
    cv::Scalar mean(0.485, 0.456, 0.406);
    cv::Scalar std(0.229, 0.224, 0.225);
    imgResized = (imgResized - mean) / std;

    // HWC -> NCHW
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
        
        cv::Mat depthFinal;
        cv::resize(depthMap, depthFinal, img.size(), 0, 0, cv::INTER_CUBIC);
        return depthFinal.clone();
    } catch (...) { return cv::Mat(); }
}

bool AIReconstructionManager::saveDepthAsPNG(const QString &path, const cv::Mat &depth32f) {
    double minVal, maxVal;
    cv::minMaxLoc(depth32f, &minVal, &maxVal);
    cv::Mat depth8u;
    if (maxVal > minVal) depth32f.convertTo(depth8u, CV_8U, 255.0/(maxVal-minVal), -minVal*255.0/(maxVal-minVal));
    else depth8u = cv::Mat::zeros(depth32f.size(), CV_8U);
    return cv::imwrite(path.toStdString(), depth8u);
}

// --- 1. Poprawiona funkcja zapisu (Kolorowe PLY) ---
bool AIReconstructionManager::savePointCloudPLY(const QString &path, const std::vector<Point3D> &points)
{
    std::ofstream ofs(path.toStdString(), std::ios::binary);
    if (!ofs.is_open()) return false;

    ofs << "ply\n";
    ofs << "format ascii 1.0\n";
    ofs << "element vertex " << points.size() << "\n";
    ofs << "property float x\n";
    ofs << "property float y\n";
    ofs << "property float z\n";
    // Kluczowe dla koloru: definicja właściwości RGB
    ofs << "property uchar red\n";
    ofs << "property uchar green\n";
    ofs << "property uchar blue\n";
    ofs << "end_header\n";

    for (const auto &p : points) {
        // Rzutujemy kolory na int, żeby strumień zapisał je jako liczby (np. "255"), 
        // a nie jako znaki ASCII.
        ofs << p.x << " " << p.y << " " << p.z << " " 
            << (int)p.r << " " << (int)p.g << " " << (int)p.b << "\n";
    }
    return true;
}

// --- 2. Poprawiona funkcja główna (Lepsza skala + Kolory) ---
void AIReconstructionManager::startAI(const QString &imagesPath, const QString &modelPath)
{
    m_timer.restart();
    
    // --- SETUP FOLDERÓW ---
    QDir sourceDir(imagesPath);
    QString folderName = sourceDir.dirName();
    sourceDir.cdUp(); 
    QString workspacePath = sourceDir.filePath(folderName + "_ai_workspace");
    QDir().mkpath(workspacePath);
    
    emit progressUpdated("Workspace: " + workspacePath, 0);
    qDebug() << "AI Workspace:" << workspacePath; // Log do terminala

    if (!loadModel(modelPath)) return;

    QDir imgDir(imagesPath);
    imgDir.setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    QFileInfoList files = imgDir.entryInfoList(QDir::Files);
    
    if (files.isEmpty()) { emit errorOccurred("Brak zdjęć."); return; }

    int total = files.size();

    for (int i = 0; i < total; ++i) {
        QFileInfo fi = files[i];
        QString msg = QString("AI [%1/%2]: %3").arg(i+1).arg(total).arg(fi.fileName());
        
        emit progressUpdated(msg, -1); // Do GUI
        qDebug() << qPrintable(msg);   // Do terminala

        cv::Mat img = cv::imread(fi.absoluteFilePath().toStdString());
        if (img.empty()) continue;

        cv::Mat imgRGB;
        cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);

        cv::Mat depth = runInference(img);
        if (depth.empty()) continue;

        saveDepthAsPNG(workspacePath + "/" + fi.completeBaseName() + "_depth.png", depth);

        // --- GENEROWANIE CHMURY (POPRAWIONE) ---
        std::vector<Point3D> currentPoints;

        // Skala głębi:
        // MiDaS zwraca odwrotność głębi. Wartości mogą być duże.
        // X i Y to piksele (np. 0..4000).
        // Żeby proporcje były naturalne, Z też musi być w okolicach 0..4000, a nie 100000.
        // Ustawiamy mnożnik, który "uspokaja" głębię.
        float depthFactor = 500.0f; 

        for (int y = 0; y < depth.rows; y += m_subsample) {
            for (int x = 0; x < depth.cols; x += m_subsample) {
                float d = depth.at<float>(y, x);
                
                // Odsiewamy tło
                if (d < 0.0001) continue; 

                // POPRAWNA MATEMATYKA:
                // X i Y zostawiamy w pikselach (duże liczby)
                float px = (float)x;
                float py = (float)y;
                
                // Z skalujemy w dół, żeby pasowało do X i Y
                float pz = (1.0f / d) * depthFactor;

                // Opcjonalnie: Obetnij sufit (zbyt dalekie punkty), żeby nie było "igieł"
                if (pz > 5000.0f) pz = 5000.0f;

                Point3D p;
                p.x = px;
                p.y = py;
                p.z = pz;

                cv::Vec3b color = imgRGB.at<cv::Vec3b>(y, x);
                p.r = color[0];
                p.g = color[1];
                p.b = color[2];

                currentPoints.emplace_back(p);
            }
        }

        QString plyPath = workspacePath + "/" + fi.completeBaseName() + ".ply";
        savePointCloudPLY(plyPath, currentPoints);

        int percent = ((i + 1) * 100) / total;
        emit progressUpdated("", percent);
    }

    emit finished(workspacePath);
    qDebug() << "AI Finished. Saved in:" << workspacePath;
}
