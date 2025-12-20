#include "ai_worker.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include <fstream>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <algorithm> // dla std::max, std::min

#include <onnxruntime_cxx_api.h>

AIWorker::AIWorker()
{
}

AIWorker::~AIWorker()
{
}

// ==========================
//  Pomocnicze struktury
// ==========================

struct Point3D {
    float x, y, z;
    unsigned char r, g, b;
};

int AIWorker::run(const QString& imagesPath,
                  const QString& modelPath,
                  const QString& outputPath)
{
    qInfo() << "AI worker started";

    // --- Foldery ---
    QDir sourceDir(imagesPath);
    if (!sourceDir.exists()) {
        qCritical() << "Brak folderu ze zdjęciami";
        return 1;
    }

    QDir().mkpath(outputPath);
    qInfo() << "Workspace:" << outputPath;

    // ==========================
    //  ŁADOWANIE MODELU
    // ==========================

    // Ustawiamy poziom logowania ORT
    Ort::Env ortEnv(ORT_LOGGING_LEVEL_WARNING, "ImageTo3D_AI");

    if (!QFile::exists(modelPath)) {
        qCritical() << "Brak pliku modelu:" << modelPath;
        return 1;
    }

    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    Ort::Session session(
        ortEnv,
        modelPath.toStdString().c_str(),
        sessionOptions
    );

    // Detekcja wejść/wyjść i rozmiaru
    Ort::AllocatorWithDefaultOptions allocator;

    auto inName  = session.GetInputNameAllocated(0, allocator);
    auto outName = session.GetOutputNameAllocated(0, allocator);

    const char* inputName  = inName.get();
    const char* outputName = outName.get();

    auto typeInfo   = session.GetInputTypeInfo(0);
    auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
    auto dims       = tensorInfo.GetShape();

    int inputHeight = (dims.size() >= 4 && dims[2] > 0) ? dims[2] : 384;
    int inputWidth  = (dims.size() >= 4 && dims[3] > 0) ? dims[3] : 384;

    qInfo() << "ORT: Wykryto rozdzielczość:"
            << inputWidth << "x" << inputHeight;

    // ==========================
    //  WCZYTANIE OBRAZÓW
    // ==========================

    QDir imgDir(imagesPath);
    imgDir.setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    QFileInfoList files = imgDir.entryInfoList(QDir::Files);

    if (files.isEmpty()) {
        qCritical() << "Brak zdjęć.";
        return 1;
    }

    int total = files.size();

    // ==========================
    //  GŁÓWNA PĘTLA
    // ==========================

    for (int i = 0; i < total; ++i) {
        QFileInfo fi = files[i];

        qInfo() << "AI [" << (i + 1) << "/" << total << "]:" << fi.fileName();

        cv::Mat img = cv::imread(fi.absoluteFilePath().toStdString());
        if (img.empty()) continue;

        // --- PREPROCESSING ---
        cv::Mat imgRGB, imgResized;
        cv::cvtColor(img, imgRGB, cv::COLOR_BGR2RGB);
        cv::resize(imgRGB, imgResized,
                   cv::Size(inputWidth, inputHeight),
                   0, 0, cv::INTER_CUBIC);

        // Normalizacja do [0,1]
        imgResized.convertTo(imgResized, CV_32F, 1.0f / 255.0f);

        // Dla DPT i MiDaS często stosuje się normalizację ImageNet
        cv::Scalar mean(0.485, 0.456, 0.406);
        cv::Scalar std (0.229, 0.224, 0.225);
        imgResized = (imgResized - mean) / std;

        // HWC -> NCHW
        cv::Mat blob = cv::dnn::blobFromImage(imgResized);

        size_t inputTensorSize = 1 * 3 * inputHeight * inputWidth;
        std::vector<int64_t> inputDims = {1, 3, inputHeight, inputWidth};

        Ort::MemoryInfo memInfo =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value inputTensor =
            Ort::Value::CreateTensor<float>(
                memInfo,
                blob.ptr<float>(),
                inputTensorSize,
                inputDims.data(),
                inputDims.size()
            );

        // --- INFERENCJA ---
        auto outputTensors = session.Run(
            Ort::RunOptions{nullptr},
            &inputName,
            &inputTensor,
            1,
            &outputName,
            1
        );

        float* floatArr =
            outputTensors.front().GetTensorMutableData<float>();

        cv::Mat depthMap(inputHeight, inputWidth, CV_32F, floatArr);

        // --- NORMALIZACJA ---
        double minVal, maxVal;
        cv::minMaxLoc(depthMap, &minVal, &maxVal);

        cv::Mat depthNormalized;
        if (maxVal - minVal > 1e-6) {
            depthMap.convertTo(
                depthNormalized,
                CV_32F,
                1.0 / (maxVal - minVal),
                -minVal / (maxVal - minVal)
            );
        } else {
            depthNormalized = cv::Mat::zeros(depthMap.size(), CV_32F);
        }

        cv::Mat depthFinal;
        cv::resize(depthNormalized, depthFinal,
                   img.size(), 0, 0, cv::INTER_CUBIC);

        // --- ZAPIS PNG ---
        cv::Mat depth8u;
        depthFinal.convertTo(depth8u, CV_8U, 255.0);
        cv::imwrite(
            (outputPath + "/" + fi.completeBaseName() + "_depth.png").toStdString(),
            depth8u
        );

        // --- CHMURA PUNKTÓW ---
        std::vector<Point3D> points;

        float centerX = depthFinal.cols / 2.0f;
        float centerY = depthFinal.rows / 2.0f;

        float focalLength = 1000.0f;
        float depthScale  = 2000.0f;

        for (int y = 0; y < depthFinal.rows; y += 2) {
            for (int x = 0; x < depthFinal.cols; x += 2) {

                float d = depthFinal.at<float>(y, x);
                if (d < 0.1f) continue;

                float Z = (1.0f / (d + 0.01f)) * depthScale;
                Z = std::min(Z, 5000.0f);

                float X = (x - centerX) * Z / focalLength;
                float Y = (y - centerY) * Z / focalLength;

                cv::Vec3b c = imgRGB.at<cv::Vec3b>(y, x);

                points.push_back({X, Y, Z, c[0], c[1], c[2]});
            }
        }

        // --- ZAPIS PLY ---
        QString plyPath = outputPath + "/" + fi.completeBaseName() + ".ply";
        std::ofstream ofs(plyPath.toStdString());

        ofs << "ply\nformat ascii 1.0\n";
        ofs << "element vertex " << points.size() << "\n";
        ofs << "property float x\nproperty float y\nproperty float z\n";
        ofs << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
        ofs << "end_header\n";

        for (const auto& p : points) {
            ofs << p.x << " " << p.y << " " << p.z << " "
                << int(p.r) << " " << int(p.g) << " " << int(p.b) << "\n";
        }
    }

    qInfo() << "AI worker finished";
    return 0;
}
