#ifndef ONNXRECONSTRUCTIONMANAGER_H
#define ONNXRECONSTRUCTIONMANAGER_H

#include "IReconstructionManager.h"
#include <QElapsedTimer>
#include <QString>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <atomic>

class OnnxReconstructionManager : public IReconstructionManager
{
    Q_OBJECT
public:
    explicit OnnxReconstructionManager(QObject *parent = nullptr);
    ~OnnxReconstructionManager();

    // IReconstructionManager interface
    void startReconstruction(const QString &imagesPath, const QString &outputPath) override;

public slots:
    // --- FIX: To musi być SLOT, aby działało invokeMethod z innego wątku ---
    void setModelPath(const QString &path);
    void cancel() override;

private:
    struct Point3D {
        float x, y, z;
        uint8_t r, g, b;
    };
    enum ModelType {
        MiDaS,
        DPT,
        UNKNOWN
    };
    
    bool loadModel(const QString &path);
    cv::Mat runInference(const cv::Mat &img);
    bool saveDepthAsPNG(const QString &path, const cv::Mat &depth32f);
    bool savePointCloudPLY(const QString &path, const std::vector<Point3D> &points);
    void analyzeDepthMap(const cv::Mat& depth);

    bool m_modelLoaded = false;
    QString m_currentModelPath;
    QString m_targetModelPath;
    QElapsedTimer m_timer;
    ModelType m_modelType = UNKNOWN;

    std::shared_ptr<Ort::Env> m_ortEnv;
    std::shared_ptr<Ort::Session> m_session;
    
    int m_inputWidth = 256;
    int m_inputHeight = 256;
    std::vector<const char*> m_inputNames;
    std::vector<const char*> m_outputNames;
    std::vector<std::string> m_inputNameStrings;
    std::vector<std::string> m_outputNameStrings;
    
    int m_subsample = 4; 
    std::atomic<bool> m_stopRequested{false};
};

#endif // ONNXRECONSTRUCTIONMANAGER_H
