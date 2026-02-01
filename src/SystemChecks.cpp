#include "SystemChecks.h"
#include <QProcess>
#include <QDebug>

bool SystemChecks::checkCudaAvailable() {
    // Zamiast linkować biblioteki CUDA, po prostu sprawdzamy,
    // czy narzędzie diagnostyczne Nvidii jest dostępne i działa.
    // To działa idealnie w Dockerze.
    
    QProcess process;
    process.start("nvidia-smi", QStringList() << "-L");
    process.waitForFinished();

    if (process.exitCode() == 0) {
        qDebug() << "[SystemChecks] Wykryto GPU Nvidia (nvidia-smi działa).";
        return true;
    } else {
        qDebug() << "[SystemChecks] Nie wykryto GPU (nvidia-smi zwróciło błąd lub brak komendy).";
        return false;
    }
}
