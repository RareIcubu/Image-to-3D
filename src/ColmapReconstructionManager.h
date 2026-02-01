#ifndef COLMAPRECONSTRUCTIONMANAGER_H
#define COLMAPRECONSTRUCTIONMANAGER_H

#include "IReconstructionManager.h"
#include <QProcess>
#include <QElapsedTimer>

class ColmapReconstructionManager : public IReconstructionManager {
    Q_OBJECT

public:
    explicit ColmapReconstructionManager(QObject *parent = nullptr);

    void startReconstruction(const QString &imagesPath, const QString &outputPath) override;

public slots:
    void cancel() override;

private:
    void runNextStep();

    QElapsedTimer m_stepTimer;
    QString m_workspacePath;
    QString m_imagesPath;
    int m_currentStep = 0;
    QProcess *m_process;
    bool m_useGpu;
    bool m_useFastMode = true; 
    bool m_isCanceled = false;

    // Parametry jakości
    int m_maxImageSize = 2000;
    int m_minInliers = 15;
    int m_numThreads = -1;
    int m_maxFeatures = 8192;
    bool m_useGeomConsistency = true;
    QString m_outputFormat = "PLY";
};

#endif // COLMAPRECONSTRUCTIONMANAGER_H
