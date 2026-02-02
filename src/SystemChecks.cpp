#include "SystemChecks.h"
#include <QProcess>
#include <QDebug>

bool SystemChecks::s_checked = false;
bool SystemChecks::s_isCudaAvailable = false;

bool SystemChecks::checkCudaAvailable() {
    if (s_checked) return s_isCudaAvailable;

    QProcess process;
    // Sprawdzamy obecność nvidia-smi
    process.start("nvidia-smi", QStringList() << "-L");
    process.waitForFinished();

    if (process.exitCode() == 0) {
        qDebug() << "[SystemChecks] Wykryto GPU Nvidia (nvidia-smi działa).";
        s_isCudaAvailable = true;
    } else {
        qDebug() << "[SystemChecks] Nie wykryto GPU lub sterownika (nvidia-smi fail). Fallback to CPU.";
        s_isCudaAvailable = false;
    }
    
    s_checked = true;
    return s_isCudaAvailable;
}
