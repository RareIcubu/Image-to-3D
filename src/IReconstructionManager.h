#ifndef IRECONSTRUCTIONMANAGER_H
#define IRECONSTRUCTIONMANAGER_H

#include <QObject>
#include <QString>

class IReconstructionManager : public QObject {
    Q_OBJECT

public:
    explicit IReconstructionManager(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IReconstructionManager() = default;

    // Abstract method to start reconstruction
    virtual void startReconstruction(const QString &inputPath, const QString &outputPath) = 0;

public slots:
    // Common method to request cancellation
    virtual void cancel() = 0;

signals:
    void progressUpdated(QString step, int percentage);
    void finished(QString modelPath);
    void errorOccurred(QString message);
};

#endif // IRECONSTRUCTIONMANAGER_H
