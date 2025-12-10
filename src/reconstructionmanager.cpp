#include "reconstructionmanager.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <cstdlib>

ReconstructionManager::ReconstructionManager(QObject *parent) : QObject(parent) {
    m_process = nullptr;
    m_fallbackToCpu = false;
    m_useFastMode = true;
}

void ReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    // --- FIX: TEJ LINII BRAKOWAŁO ---
    m_imagesPath = imagesPath; 
    // --------------------------------
    
    m_workspacePath = outputDir;
    m_currentStep = 0; // Startujemy od Undistortera (jak chciałeś)
    m_fallbackToCpu = false;

    // 1. Sprawdzenie ścieżek
    QDir imgDir(m_imagesPath); // Używamy m_imagesPath
    QDir workDir(m_workspacePath);

    if (imgDir.canonicalPath() == workDir.canonicalPath()) {
        emit errorOccurred("BŁĄD KRYTYCZNY: Folder wyjściowy nie może być ten sam co folder ze zdjęciami!");
        return;
    }

    if (!QDir().mkpath(m_workspacePath)) {
        emit errorOccurred("Nie udało się stworzyć folderu: " + m_workspacePath);
        return;
    }
    
    qDebug() << "Reconstruction started using images in:" << m_imagesPath;
    
    // Uruchamiamy proces
    runNextStep();
}
void ReconstructionManager::runNextStep() {
    m_stepTimer.restart();

    QString colmapBinary = "colmap";
    QStringList colmapArgs;

    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };

    // --- DETEKCJA ŚRODOWISKA NVIDIA (FIX) ---
    bool useNvidiaFix = false;
    const char* envVar = std::getenv("NVIDIA_DOCKER_FIX");
    if (envVar && QString(envVar) == "1") {
        useNvidiaFix = true;
        qDebug() << "[INFO] Wykryto tryb Nvidia Docker Fix: Wymuszam CPU dla SIFT i limit pamięci.";
    } else {
        qDebug() << "[INFO] Tryb standardowy: Pełne GPU.";
    }
    // ----------------------------------------

    // --- KONFIGURACJA KROKÓW ---
    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Extracting Features...\n", 10);
        colmapArgs << "feature_extractor"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath);

        if (useNvidiaFix) {
            // FIX DLA NVIDIA: CPU + Mniejsze zdjęcia (Brak Crashy, Brak OOM)
            colmapArgs << "--SiftExtraction.use_gpu=1";
            colmapArgs << "--SiftExtraction.max_image_size=1600";
            //colmapArgs << "--FeatureExtraction.num_threads" << "4";

        } else {
            // STANDARD (AMD/WINDOWS): Pełne GPU, pełna jakość
            colmapArgs << "--SiftExtraction.use_gpu=1";
            // Domyślny rozmiar (3200) lub brak limitu
        }
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Matching Features...\n", 25);
        colmapArgs << "exhaustive_matcher"
                   << arg("database_path", m_workspacePath + "/database.db");

        if (useNvidiaFix) {
            //colmapArgs << "--FeatureMatching.use_gpu=0"; // CPU dla stabilności
        } else {
            colmapArgs << "--SiftMatching.use_gpu=1"; // GPU dla szybkości
        }
        break;

    case 2: // SPARSE RECONSTRUCTION
        emit progressUpdated("Sparse Reconstruction...\n", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        colmapArgs << "mapper"
                   << arg("database_path", m_workspacePath + "/database.db")
                   << arg("image_path", m_imagesPath)
                   << arg("output_path", m_workspacePath + "/sparse")
                    << "--Mapper.tri_ignore_two_view_tracks" << "0"

                   // 2. ZEJDŹ DO ABSOLUTNEGO MINIMUM PUNKTÓW
                   // Jeśli znajdzie chociaż 10 punktów, niech startuje.
                   << "--Mapper.init_min_num_inliers" << "10"
                   
                   // 3. DAJ WIĘCEJ CZASU NA OBLICZENIA MATEMATYCZNE (BA)
                   // Zwiększamy liczbę iteracji, żeby matematyka "zdążyła" się zbiec.
                   //<< "--Mapper.ba_local_max_num_iterations" << "50"
                   
                   // 4. MNIEJSZY KĄT (Dla płynnych przejść wideo/turntable)
                   << "--Mapper.init_min_tri_angle" << "4";
        break;

    case 3: // MODEL CONVERTER / UNDISTORTER
        if (m_useFastMode) {
            emit progressUpdated("Exporting Point Cloud...\n", 90);
            colmapArgs << "model_converter"
                       << arg("input_path", m_workspacePath + "/sparse/0")
                       << arg("output_path", m_workspacePath + "/model.ply")
                       << arg("output_type", "PLY");
            m_currentStep = 99;
        }else {
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
        
        // Definiujemy argumenty TYLKO RAZ
        colmapArgs << "patch_match_stereo"
                   << arg("workspace_path", m_workspacePath + "/dense")
                   << arg("workspace_format", "COLMAP")
                   << arg("PatchMatchStereo.geom_consistency", "true");
        break; // <--- To kończy case 4. Wszystko pod spodem usuń!
  case 5: // FUSION
        emit progressUpdated("Fusing Point Cloud...\n", 85);
        
        // --- FIX NA WIELKOŚĆ LITER (JPG vs jpg) ---
        // To jest mały hack, który kopiuje/linkuje pliki, jeśli rozszerzenia się nie zgadzają.
        // Wykonujemy to wewnątrz C++ (Qt) przed odpaleniem procesu.
        {
            QDir denseImagesDir(m_workspacePath + "/dense/images");
            QStringList files = denseImagesDir.entryList(QDir::Files);
            for (const QString &file : files) {
                if (file.endsWith(".jpg")) {
                    QString upper = file; 
                    upper.replace(".jpg", ".JPG");
                    if (!denseImagesDir.exists(upper)) {
                        QFile::link(denseImagesDir.filePath(file), denseImagesDir.filePath(upper));
                    }
                }
            }
        }
        // ------------------------------------------

        colmapArgs << "stereo_fusion"
                   << arg("workspace_path", m_workspacePath + "/dense")                   
                   << arg("workspace_format", "COLMAP")
                   << arg("input_type", "photometric")
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

    // --- WRAPPER SCRIPT (Dla logów COLMAP) ---
    QString fullCommand = colmapBinary + " " + colmapArgs.join(" ");
    QString program = "/usr/bin/script";
    QStringList scriptArgs;
    scriptArgs << "-q" << "-e" << "-c" << fullCommand << "/dev/null";

    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        while (m_process->canReadLine()) {
            QByteArray data = m_process->readLine();
            QString line = QString::fromLocal8Bit(data).trimmed();
            
            // (Tutaj Twój kod parsowania paska postępu - bez zmian)
            static QRegularExpression reProgress("\\[(\\d+)/(\\d+)\\]");
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
