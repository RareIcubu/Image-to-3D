#ifndef AI_RECONSTRUCTIONMANAGER_H
#define AI_RECONSTRUCTIONMANAGER_H

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

class AIReconstructionManager : public QObject
{
    Q_OBJECT
public:
    explicit AIReconstructionManager(QObject *parent = nullptr);
    ~AIReconstructionManager();

public slots:
    void startAI(const QString &imagesPath, const QString &modelPath);

signals:
    void progressUpdated(const QString &msg, int percentage);
    void errorOccurred(const QString &msg);
    void finished(const QString &resultPath);

private:
    struct Point3D {
        float x, y, z;
        uint8_t r, g, b;
    };
    enum ModelType{
        MiDaS,
        DPT,
        UNKNOWN
    };
    bool loadModel(const QString &path);
    cv::Mat runInference(const cv::Mat &img);
    bool saveDepthAsPNG(const QString &path, const cv::Mat &depth32f);
    bool savePointCloudPLY(const QString &path, const std::vector<Point3D> &points);
    void analyzeDepthMap(const cv::Mat& depth);
private:
    bool m_modelLoaded = false;
    QString m_currentModelPath;
    QElapsedTimer m_timer;

    ModelType m_modelType = UNKNOWN;

    std::shared_ptr<Ort::Env> m_ortEnv;
    std::shared_ptr<Ort::Session> m_session;
    
    int m_inputWidth = 256;
    int m_inputHeight = 256;
    std::vector<const char*> m_inputNames;
    std::vector<const char*> m_outputNames;
    // Przechowujemy stringi nazw, bo c_str() musi wskazywać na żywą pamięć
    std::vector<std::string> m_inputNameStrings;
    std::vector<std::string> m_outputNameStrings;
    
    int m_subsample = 4; 
};

#endif // AI_RECONSTRUCTIONMANAGER_H
