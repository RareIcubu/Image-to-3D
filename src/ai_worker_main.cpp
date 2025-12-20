#include "ai_worker.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Oczekiwane argumenty:
    // argv[1] = imagesPath
    // argv[2] = modelPath
    // argv[3] = outputPath
    if (argc != 4) {
        qCritical().noquote()
        << "Usage:\n"
        << "  ai_worker <imagesPath> <modelPath> <outputPath>\n"
        << "Received arguments:" << argc - 1;
        return 1;
    }

    const QString imagesPath = QString::fromLocal8Bit(argv[1]);
    const QString modelPath  = QString::fromLocal8Bit(argv[2]);
    const QString outputPath = QString::fromLocal8Bit(argv[3]);

    qInfo() << "AI WORKER START";
    qInfo() << "Images:" << imagesPath;
    qInfo() << "Model :" << modelPath;
    qInfo() << "Output:" << outputPath;

    AIWorker worker;
    int result = worker.run(imagesPath, modelPath, outputPath);

    qInfo() << "AI WORKER EXIT CODE:" << result;
    return result;
}
