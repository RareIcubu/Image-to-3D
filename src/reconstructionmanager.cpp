#include "reconstructionmanager.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>

ReconstructionManager::ReconstructionManager(QObject *parent) : QObject(parent) {
    m_process = nullptr;
    m_fallbackToCpu = false;
    m_useFastMode = true;
}

void ReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    m_imagesPath = imagesPath;
    m_workspacePath = outputDir;
    m_currentStep = 0;
    m_fallbackToCpu = false;

    QDir imgDir(m_imagesPath);
    QDir workDir(m_workspacePath);

    if (imgDir.canonicalPath() == workDir.canonicalPath()) {
        emit errorOccurred("BŁĄD KRYTYCZNY: Folder wyjściowy nie może być ten sam co folder ze zdjęciami!");
        return;
    }

    // Czyszczenie
    if (workDir.exists()) {
        qDebug() << "Wykryto stary workspace. Usuwanie...";
        if (!workDir.removeRecursively()) {
            emit errorOccurred("Nie udało się usunąć starego workspace'u.");
            return;
        }
    }

    if (!QDir().mkpath(m_workspacePath)) {
        emit errorOccurred("Nie udało się stworzyć folderu: " + m_workspacePath);
        return;
    }

    qDebug() << "Starting reconstruction in:" << m_workspacePath;
    runNextStep();
}

void ReconstructionManager::runNextStep() {
    m_stepTimer.restart();

    QString colmapBinary = "/usr/bin/colmap";
    QStringList colmapArgs;

    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };

    // --- KONFIGURACJA KROKÓW ---
    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Extracting Features...\n", 10);
        colmapArgs << "feature_extractor"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath);
                   // Usunięto flagi use_gpu/num_threads (zgodnie z życzeniem)
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Matching Features...\n", 25);
        colmapArgs << "exhaustive_matcher"
                   << arg("database_path", m_workspacePath + "/database.db");
        break;

    case 2: // SPARSE RECONSTRUCTION
        emit progressUpdated("Sparse Reconstruction...\n", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        colmapArgs << "mapper"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath)
                   << arg("output_path", m_workspacePath + "/sparse");
        break;

    case 3: // MODEL CONVERTER / UNDISTORTER
        if (m_useFastMode) {
            emit progressUpdated("Exporting Point Cloud...\n", 90);
            colmapArgs << "model_converter"
                       << arg("input_path", m_workspacePath + "/sparse/0")
                       << arg("output_path", m_workspacePath + "/model.ply")
                       << arg("output_type", "PLY");
            m_currentStep = 99;
        } else {
            emit progressUpdated("Undistorting Images...\n", 55);
            QDir(m_workspacePath + "/dense").mkpath(".");
            colmapArgs << "image_undistorter"
                       << arg("image_path", m_imagesPath)
                       << arg("input_path", m_workspacePath + "/sparse/0")
                       << arg("output_path", m_workspacePath + "/dense")
                       << arg("output_type", "COLMAP")
                       << arg("max_image_size", "2000");
        }
        break;

    case 4: // DENSE STEREO
        emit progressUpdated("Calculating Depth Maps...\n", 70);
        colmapArgs << "patch_match_stereo"
                   << arg("workspace_path", m_workspacePath + "/dense")
                   << arg("workspace_format", "COLMAP")
                   // Domyślne ustawienia COLMAP (bez wymuszania GPU/CPU)
                   << arg("PatchMatchStereo.geom_consistency", "true");
        break;

    case 5: // FUSION
        emit progressUpdated("Fusing Point Cloud...\n", 85);
        colmapArgs << "stereo_fusion"
                   << arg("workspace_path", m_workspacePath + "/dense")
                   << arg("workspace_format", "COLMAP")
                   << arg("input_type", "geometric")
                   << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // POISSON MESHING
        emit progressUpdated("Generating Mesh...\n", 90);
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

    // --- WRAPPER SCRIPT (Naprawa Logów) ---
    QString fullCommand = colmapBinary + " " + colmapArgs.join(" ");
    QString program = "/usr/bin/script";
    QStringList scriptArgs;
    // -q: Quiet, -e: Return exit code, -c: Command, /dev/null: Output sink
    scriptArgs << "-q" << "-e" << "-c" << fullCommand << "/dev/null";

    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        while (m_process->canReadLine()) {
            QByteArray data = m_process->readLine();
            QString line = QString::fromLocal8Bit(data).trimmed();
            
            // Parsowanie postępu
            static QRegularExpression reProgress("\\[(\\d+)/(\\d+)\\]");
            QRegularExpressionMatch match = reProgress.match(line);
            int pct = -1;

            if (match.hasMatch()) {
                int cur = match.captured(1).toInt();
                int tot = match.captured(2).toInt();
                if (tot > 0) {
                    // Proste skalowanie paska
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
            [=](int exitCode, QProcess::ExitStatus status) {
        if (exitCode == 0) {
            if (m_currentStep == 99) m_currentStep = 100;
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
