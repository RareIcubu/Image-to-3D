#include "mvs_runner.h"
#include <QDir>
#include <QFileInfo>

MvsRunner::MvsRunner(QObject* parent)
    : QObject(parent)
{
    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit log(QString::fromUtf8(m_process->readAllStandardOutput()).trimmed());
    });

    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        emit log(QString::fromUtf8(m_process->readAllStandardError()).trimmed());
    });
}

MvsRunner::~MvsRunner()
{
    stop();
}

void MvsRunner::stop()
{
    m_abort = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();   // HARD stop (MVS is RAM-heavy)
        m_process->waitForFinished();
    }
}

bool MvsRunner::runStep(const QStringList& args)
{
    if (m_abort) return false;

    m_process->start("colmap", args);

    if (!m_process->waitForStarted()) {
        emit error("Failed to start COLMAP");
        return false;
    }

    if (!m_process->waitForFinished(-1)) {
        emit error("COLMAP step crashed");
        return false;
    }

    if (m_process->exitCode() != 0) {
        emit error("COLMAP exited with error");
        return false;
    }

    return true;
}

void MvsRunner::startMvs(const QString& imagesPath,
                         const QString& workspacePath)
{
    m_abort = false;

    QString sparsePath = workspacePath + "/sparse/0";
    QString densePath  = workspacePath + "/dense";

    if (!QDir(imagesPath).exists()) {
        emit error("Images folder does not exist");
        return;
    }

    if (!QDir(sparsePath).exists()) {
        emit error("Sparse model not found (sparse/0)");
        return;
    }

    QDir().mkpath(densePath);

    emit log("=== COLMAP MVS START ===");

    // 1. Image undistortion
    emit log("[1/3] Image undistortion...");
    if (!runStep(QStringList{
            "image_undistorter",
            "--image_path", imagesPath,
            "--input_path", sparsePath,
            "--output_path", densePath,
            "--output_type", "COLMAP"
        })) return;

    // 2. PatchMatch stereo
    emit log("[2/3] PatchMatch stereo...");
    if (!runStep(QStringList{
            "patch_match_stereo",
            "--workspace_path", densePath,
            "--workspace_format", "COLMAP",
            "--PatchMatchStereo.geom_consistency", "true"
        })) return;

    if (!QFile::exists(densePath + "/stereo/fusion.cfg")) {
        emit error("fusion.cfg not created — PatchMatch failed");
        return;
    }

    // 3. Stereo fusion
    emit log("[3/3] Stereo fusion...");
    QString fusedPath = densePath + "/fused.ply";

    if (!runStep(QStringList{
            "stereo_fusion",
            "--workspace_path", densePath,
            "--workspace_format", "COLMAP",
            "--input_type", "geometric",
            "--output_path", fusedPath
        })) return;

    emit log("=== MVS FINISHED ===");
    emit finished(fusedPath);
}
