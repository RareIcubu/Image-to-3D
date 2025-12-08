#include "ai_reconstructionmanager.h"
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <fstream>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <algorithm> // dla std::max, std::min
#include <QThread>

AIReconstructionManager::AIReconstructionManager(QObject *parent) : QObject(parent) {
    m_modelLoaded = false;
    // Ustawiamy poziom logowania ORT
    m_ortEnv = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ImageTo3D_AI");
     m_abort.store(false); // Zatrzymywanie
}

AIReconstructionManager::~AIReconstructionManager() {
    // request stop and give a short time for operations to finish
    stop();
    // If session holds resources, release it
    m_session.reset();
}

void AIReconstructionManager::stop() {
    m_abort.store(true);
}

bool AIReconstructionManager::loadModel(const QString &modelPath) {
    if (m_abort.load()) return false; // Zatrzymywanie

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

         if (m_abort.load()) return false; // Zatrzymywanie

        m_session = std::make_shared<Ort::Session>(*m_ortEnv, modelPath.toStdString().c_str(), sessionOptions);

        // Detekcja wejść/wyjść i rozmiaru
        Ort::AllocatorWithDefaultOptions allocator;

        m_inputNameStrings.clear();
        m_outputNameStrings.clear();
        m_inputNames.clear();
        m_outputNames.clear();

        // Wejście
        auto inName = m_session->GetInputNameAllocated(0, allocator);
        m_inputNameStrings.push_back(inName.get());
        m_inputNames.push_back(m_inputNameStrings.back().c_str());

        // Wyjście
        auto outName = m_session->GetOutputNameAllocated(0, allocator);
        m_outputNameStrings.push_back(outName.get());
        m_outputNames.push_back(m_outputNameStrings.back().c_str());

        // Rozmiar
        auto typeInfo = m_session->GetInputTypeInfo(0);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        auto dims = tensorInfo.GetShape();

        if (dims.size() >= 4) {
            // Jeśli dynamiczny (-1), ustaw domyślnie 384
            m_inputHeight = (dims[2] > 0) ? dims[2] : 384;
            m_inputWidth  = (dims[3] > 0) ? dims[3] : 384;
            qDebug() << "ORT: Wykryto rozdzielczość:" << m_inputWidth << "x" << m_inputHeight;
        }

        // Detekcja typu modelu (dla logów, bo normalizację zrobimy uniwersalną)
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

cv::Mat AIReconstructionManager::runInference(const cv::Mat &img) {
    if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
    if (!m_modelLoaded) return cv::Mat();

    // 1. Preprocessing
    if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
    cv::Mat imgRGB, imgResized;
    if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
    cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);
    cv::resize(imgRGB, imgResized, cv::Size(m_inputWidth, m_inputHeight), 0, 0, cv::INTER_CUBIC);

    // Normalizacja do [0,1]
    imgResized.convertTo(imgResized, CV_32F, 1.0f / 255.0f);

    // Dla DPT i MiDaS często stosuje się normalizację ImageNet
    cv::Scalar mean(0.485, 0.456, 0.406);
    cv::Scalar std(0.229, 0.224, 0.225);
    if (m_abort.load()) return cv::Mat();
    imgResized = (imgResized - mean) / std; // Zatrzymywanie

    // HWC -> NCHW
    if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
    cv::Mat blob = cv::dnn::blobFromImage(imgResized);

    if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
    size_t inputTensorSize = 1 * 3 * m_inputHeight * m_inputWidth;
    std::vector<int64_t> inputDims = {1, 3, m_inputHeight, m_inputWidth};

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, blob.ptr<float>(), inputTensorSize, inputDims.data(), inputDims.size()
    );

    try {
        // 2. Inferencja
        if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
        auto outputTensors = m_session->Run(
            Ort::RunOptions{nullptr}, m_inputNames.data(), &inputTensor, 1, m_outputNames.data(), 1
        );
        if (m_abort.load()) return cv::Mat(); // Zatrzymywanie

        float* floatArr = outputTensors.front().GetTensorMutableData<float>();
        cv::Mat depthMap(m_inputHeight, m_inputWidth, CV_32F, floatArr);

        // 3. UNIWERSALNA NORMALIZACJA (Klucz do sukcesu)
        // Sprowadzamy wszystko do zakresu 0.0 (daleko/tło) - 1.0 (blisko).
        // Dzięki temu algorytm generowania punktów zawsze działa tak samo.
        
        if (m_abort.load()) return cv::Mat();
        double minVal, maxVal;
        cv::minMaxLoc(depthMap, &minVal, &maxVal);
        
        if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
        cv::Mat depthNormalized;
        if (maxVal - minVal > 1e-6) {
            // Wzór: (val - min) / (max - min)
            depthMap.convertTo(depthNormalized, CV_32F, 1.0f / (maxVal - minVal), -minVal / (maxVal - minVal));
        } else {
            depthNormalized = cv::Mat::zeros(depthMap.size(), CV_32F);
        }

        // Resize do oryginału
        if (m_abort.load()) return cv::Mat(); // Zatrzymywanie
        cv::Mat depthFinal;
        cv::resize(depthNormalized, depthFinal, img.size(), 0, 0, cv::INTER_CUBIC);
        if (m_abort.load()) return cv::Mat(); // Zatrzymywanie

        return depthFinal.clone();

    } catch (const std::exception& e) {
        qCritical() << "Inference error:" << e.what();
        return cv::Mat();
    } catch (...) {
        qCritical() << "Unknown inference error";
        return cv::Mat();
    }
}

bool AIReconstructionManager::saveDepthAsPNG(const QString &path, const cv::Mat &depth32f) {
    // Zapisujemy jako 8-bitowy obrazek do podglądu
    cv::Mat depth8u;
    depth32f.convertTo(depth8u, CV_8U, 255.0); // Skoro depth32f jest 0..1, to *255 daje 0..255
    return cv::imwrite(path.toStdString(), depth8u);
}

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

void AIReconstructionManager::analyzeDepthMap(const cv::Mat& depth) {
    double minVal, maxVal;
    cv::minMaxLoc(depth, &minVal, &maxVal);
    cv::Scalar mean, stddev;
    cv::meanStdDev(depth, mean, stddev);
    qDebug() << "Depth Stats -> Min:" << minVal << "Max:" << maxVal << "Mean:" << mean[0];
}

void AIReconstructionManager::startAI(const QString &imagesPath, const QString &modelPath)
{
    m_timer.restart();
    m_abort.store(false); // Zatrzymywanie
    
    // --- Foldery ---
    QDir sourceDir(imagesPath);
    QString folderName = sourceDir.dirName();
    sourceDir.cdUp();
    QString workspacePath = sourceDir.filePath(folderName + "_ai_workspace");
    QDir().mkpath(workspacePath);

    emit progressUpdated("Workspace: " + workspacePath, 0);
    
    if (!loadModel(modelPath)) return;

    QDir imgDir(imagesPath);
    imgDir.setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    QFileInfoList files = imgDir.entryInfoList(QDir::Files);
    
    if (files.isEmpty()) { emit errorOccurred("Brak zdjęć."); return; }

    int total = files.size();

    for (int i = 0; i < total; ++i) {
        if (m_abort.load()) {
            emit errorOccurred("Zatrzymano przez użytkownika");
            return;
        }

        QFileInfo fi = files[i];
        QString msg = QString("AI [%1/%2]: %3").arg(i+1).arg(total).arg(fi.fileName());
        emit progressUpdated(msg, -1);
        qDebug() << qPrintable(msg);

        cv::Mat img = cv::imread(fi.absoluteFilePath().toStdString());
        if (img.empty()) continue;

        if (m_abort.load()) {
            emit errorOccurred("Zatrzymano przez użytkownika");
            return;
        }

        cv::Mat imgRGB;
        cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);

        // Inferencja (zwraca znormalizowane 0..1)
        cv::Mat depth = runInference(img);
        if (m_abort.load()) {
            emit errorOccurred("Zatrzymano przez użytkownika");
            return;
        }
        if (depth.empty()) continue;
        
        analyzeDepthMap(depth);
        if (m_abort.load()) {
            emit errorOccurred("Cancelled by user");
            return;
        }
        saveDepthAsPNG(workspacePath + "/" + fi.completeBaseName() + "_depth.png", depth);

        // --- GENEROWANIE CHMURY 3D ---
        std::vector<Point3D> currentPoints;

        float centerX = depth.cols / 2.0f;
        float centerY = depth.rows / 2.0f;

        // --- PARAMETRY SKALOWANIA (TWEAK THIS!) ---
        // Focal length: Większa = mniejszy kąt widzenia, mniejsze zniekształcenia X/Y
        float focalLength = 1000.0f; 
        
        // Depth Scale: To decyduje o "wypukłości". 
        // Skoro d jest 0..1, a obraz ma np. 4000px, to Z musi być duże, żeby było widać.
        float depthScale = 2000.0f; 

        for (int y = 0; y < depth.rows; y += m_subsample) {
            if (m_abort.load()) break;
            for (int x = 0; x < depth.cols; x += m_subsample) {
                if (m_abort.load()) break;
                float d = depth.at<float>(y, x); // Wartość 0.0 - 1.0

                // Odsiewamy tło (np. wszystko co ma wartość mniejszą niż 0.1)
                if (d < 0.1f) continue; 

                // Obliczamy Z
                // d to "relative inverse depth" (1=blisko, 0=daleko)
                // Dodajemy mały epsilon, żeby nie dzielić przez 0
                float Z = (1.0f / (d + 0.01f)) * depthScale;
                
                // CLAMP: Obcinamy sufit, żeby tło nie uciekało w nieskończoność
                // Jeśli depthScale=2000, to dla d=0.1 -> Z = 20000. To za dużo.
                if (Z > 5000.0f) Z = 5000.0f;

                // Rzutowanie perspektywiczne (Inverse Pinhole)
                // To naprawia proporcje X/Y względem odległości
                float X = (x - centerX) * Z / focalLength;
                float Y = (y - centerY) * Z / focalLength;

                Point3D p;
                p.x = X;
                p.y = Y;
                p.z = Z; // Zwróć uwagę: Z rośnie w głąb (dla OpenGL -Z, tu +Z jest OK)

                cv::Vec3b color = imgRGB.at<cv::Vec3b>(y, x);
                p.r = color[0];
                p.g = color[1];
                p.b = color[2];

                currentPoints.emplace_back(p);
            }
        }
        if (m_abort.load()) {
            emit errorOccurred("Zatrzymano przez użytkownika");
            return;
        }

        QString plyPath = workspacePath + "/" + fi.completeBaseName() + ".ply";
        savePointCloudPLY(plyPath, currentPoints);

        int percent = ((i + 1) * 100) / total;
        emit progressUpdated("", percent);
    }

    if (m_abort.load()) {
        emit errorOccurred("Zatrzymano przez użytkownika");
        return;
    }

    emit finished(workspacePath);
    qDebug() << "AI Finished.";
}
