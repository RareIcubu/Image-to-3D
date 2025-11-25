#ifndef RECONSTRUCTIONMANAGER_H
#define RECONSTRUCTIONMANAGER_H

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QDebug>
#include <QElapsedTimer>

class ReconstructionManager : public QObject {
    Q_OBJECT

public:
    explicit ReconstructionManager(QObject *parent = nullptr);

public slots:
    // Marked as a SLOT to be triggered safely from the Main Thread
    void startReconstruction(const QString &imagesPath, const QString &outputPath);

signals:
    void progressUpdated(QString step, int percentage);
    void finished(QString modelPath);
    void errorOccurred(QString message);

private:
    QElapsedTimer m_stepTimer;

    void runNextStep();

    QString m_workspacePath;
    QString m_imagesPath;
    int m_currentStep = 0;
    QProcess *m_process;

    bool m_fallbackToCpu = false;

    // SET THIS TO TRUE TO SKIP DENSE RECONSTRUCTION
    bool m_useFastMode = true;
};

#endif // RECONSTRUCTIONMANAGER_H
