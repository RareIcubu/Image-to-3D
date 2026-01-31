#include "ColmapReconstructionManager.h"
#include "SettingsDialog.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <cstdlib>
#include <QDebug>

ColmapReconstructionManager::ColmapReconstructionManager(QObject *parent) 
    : IReconstructionManager(parent), m_process(nullptr) 
{
}

void ColmapReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    m_imagesPath = imagesPath;
    m_workspacePath = outputDir;
    m_currentStep = 0;
    
    // Check Settings
    bool useMVS = SettingsDialog::isMvsEnabled();
    m_useFastMode = !useMVS; // FastMode = Skip MVS
    qDebug() << "COLMAP Config -> Use MVS:" << useMVS;

    QDir imgDir(m_imagesPath);    QDir workDir(m_workspacePath);

    if (imgDir.canonicalPath() == workDir.canonicalPath()) {
        emit errorOccurred("BŁĄD KRYTYCZNY: Folder wyjściowy nie może być ten sam co folder ze zdjęciami!");
        return;
    }

    if (!QDir().mkpath(m_workspacePath)) {
        emit errorOccurred("Nie udało się stworzyć folderu: " + m_workspacePath);
        return;
    }
    
    qDebug() << "Colmap Reconstruction started using images in:" << m_imagesPath;
    m_isCanceled = false;
    runNextStep();
}

void ColmapReconstructionManager::cancel()
{
    m_isCanceled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "Canceling COLMAP process...";
#ifdef Q_OS_WIN
        m_process->kill(); // Force kill on Windows
#else
        m_process->terminate(); // Try graceful first on Linux
        if (!m_process->waitForFinished(1000))
            m_process->kill();
#endif
    }
    emit errorOccurred("Proces anulowany przez użytkownika.");
}

void ColmapReconstructionManager::runNextStep() {
    if (m_isCanceled) return;

    m_stepTimer.restart();

    QString colmapBinary = "colmap";
    QStringList colmapArgs;

    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };

    bool useNvidiaFix = false;
    const char* envVar = std::getenv("NVIDIA_DOCKER_FIX");
    if (envVar && QString(envVar) == "1") {
        useNvidiaFix = true;
        qDebug() << "[INFO] Wykryto tryb Nvidia Docker Fix: Wymuszam CPU dla SIFT i limit pamięci.";
    }

    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Extracting Features...", 10);
        colmapArgs << "feature_extractor"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath);

        if (useNvidiaFix) {
            colmapArgs << "--SiftExtraction.use_gpu=1"
                       << "--SiftExtraction.max_image_size=1600";
        } else {
            colmapArgs << "--SiftExtraction.use_gpu=1";
        }
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Matching Features...", 25);
        colmapArgs << "exhaustive_matcher"
                   << arg("database_path", m_workspacePath + "/database.db");

        if (!useNvidiaFix) {
            colmapArgs << "--SiftMatching.use_gpu=1";
        }
        break;

    case 2: // SPARSE RECONSTRUCTION
        emit progressUpdated("Sparse Reconstruction...", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        colmapArgs << "mapper"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath)
                   << arg("output_path", m_workspacePath + "/sparse")
                   << "--Mapper.tri_ignore_two_view_tracks" << "0"
                   << "--Mapper.init_min_num_inliers" << "10"
                   << "--Mapper.init_min_tri_angle" << "4";
        break;

    case 3: // MODEL CONVERTER / UNDISTORTER
        if (m_useFastMode) {
            emit progressUpdated("Exporting Point Cloud...", 90);
            colmapArgs << "model_converter"
                       << arg("input_path", m_workspacePath + "/sparse/0")
                       << arg("output_path", m_workspacePath + "/model.ply")
                       << arg("output_type", "PLY");
            m_currentStep = 99;
        } else {
            // Unreachable in current configuration but kept for future
            QDir(m_workspacePath + "/dense").mkpath(".");
            colmapArgs << "image_undistorter"
                       << arg("image_path", m_imagesPath)
                       << arg("input_path", m_workspacePath + "/sparse/0")
                       << arg("output_path", m_workspacePath + "/dense")
                       << arg("output_type", "COLMAP")
                       << arg("max_image_size", "2000");
        }
        break;

    case 4: // DENSE STEREO (Skipped in Fast Mode)
        emit progressUpdated("Calculating Depth Maps...", 70);
        colmapArgs << "patch_match_stereo"
                   << arg("workspace_path", m_workspacePath + "/dense")
                   << arg("workspace_format", "COLMAP")
                   << arg("PatchMatchStereo.geom_consistency", "true");
        break;

    case 5: // FUSION (Skipped in Fast Mode)
        emit progressUpdated("Fusing Point Cloud...", 85);
        {
            // Case-insensitive fix for Linux filesystems
            QDir denseImagesDir(m_workspacePath + "/dense/images");
            QStringList files = denseImagesDir.entryList(QDir::Files);
            for (const QString &file : files) {
                if (file.endsWith(".jpg", Qt::CaseInsensitive)) {
                     QString target = file;
                     // Ensure extension is matching what COLMAP expects if needed, 
                     // but the original code just linked .jpg to .JPG.
                     // I'll replicate the original logic slightly cleaner.
                     if (file.endsWith(".jpg")) {
                         QString upper = file; 
                         upper.replace(".jpg", ".JPG");
                         if (!denseImagesDir.exists(upper)) {
                             QFile::link(denseImagesDir.filePath(file), denseImagesDir.filePath(upper));
                         }
                     }
                }
            }
        }
        colmapArgs << "stereo_fusion"
                   << arg("workspace_path", m_workspacePath + "/dense")                   
                   << arg("workspace_format", "COLMAP")
                   << arg("input_type", "photometric")
                   << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // POISSON MESHING (Skipped in Fast Mode)
        emit progressUpdated("Generating Mesh...", 90);
        colmapArgs << "poisson_mesher"
                   << arg("input_path", m_workspacePath + "/dense/fused.ply")
                   << arg("output_path", m_workspacePath + "/model.ply");
        break;

    default:
        if (QFile::exists(m_workspacePath + "/model.ply")) {
            emit finished(m_workspacePath + "/model.ply");
        } else {
            emit errorOccurred("Koniec procesu, ale brak pliku model.ply");
        }
        return;
    }

    if (m_process) m_process->deleteLater();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    QString fullCommand = colmapBinary + " " + colmapArgs.join(" ");
    QString program = "/usr/bin/script";
    QStringList scriptArgs;
    scriptArgs << "-q" << "-e" << "-c" << fullCommand << "/dev/null";

    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        while (m_process->canReadLine()) {
            QByteArray data = m_process->readLine();
            QString line = QString::fromLocal8Bit(data).trimmed();
            
            static QRegularExpression reProgress("\\[(\\\d+)/(\\\d+)\\]");
            QRegularExpressionMatch match = reProgress.match(line);
            int pct = -1;
            if (match.hasMatch()) {
                int cur = match.captured(1).toInt();
                int tot = match.captured(2).toInt();
                if (tot > 0) {
                    int base = 0; int range = 0;
                    if (m_currentStep == 0) { base = 0; range = 15; }
                    else if (m_currentStep == 1) { base = 15; range = 15; }
                    else if (m_currentStep == 2) { base = 30; range = 25; }
                    else if (m_currentStep == 3) { base = 55; range = 5; }
                    else if (m_currentStep == 4) { base = 60; range = 25; }
                    else if (m_currentStep == 5) { base = 85; range = 10; }
                    else if (m_currentStep == 6) { base = 95; range = 5; }
                    if (range > 0) pct = base + (int)((float)cur/tot * range);
                }
            }
            if (!line.isEmpty()) emit progressUpdated(line, pct);
        }
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            if (m_currentStep == 99) m_currentStep = 100; // Finish
            else m_currentStep++;
            runNextStep();
        } else {
            QString err = m_process->readAllStandardOutput();
            emit errorOccurred("Step failed code " + QString::number(exitCode) + "\n" + err);
        }
    });

    qDebug() << "Executing via script:" << fullCommand;
    m_process->start(program, scriptArgs);
}