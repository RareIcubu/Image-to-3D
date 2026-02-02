#include "ColmapReconstructionManager.h"
#include "SettingsDialog.h"
#include "SystemChecks.h"
#include "MeshGenerator.h" // <--- 1. DODANO IMPORT
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QDebug>

ColmapReconstructionManager::ColmapReconstructionManager(QObject *parent) 
    : IReconstructionManager(parent), m_process(nullptr) 
{
}

void ColmapReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    m_imagesPath = imagesPath;
    m_workspacePath = outputDir;
    m_currentStep = 0;
    
    // --- KONFIGURACJA SPRZĘTOWA ---
    bool useMVS = SettingsDialog::isMvsEnabled();
    QString quality = SettingsDialog::getQuality();
    
    m_useGpu = SystemChecks::checkCudaAvailable();

    m_maxImageSize = SettingsDialog::getMaxImageSize();
    m_numThreads = SettingsDialog::getNumThreads();
    m_maxFeatures = SettingsDialog::getMaxFeatures();
    m_useGeomConsistency = SettingsDialog::isGeomConsistencyEnabled();
    m_outputFormat = SettingsDialog::getOutputFormat();
    
    m_minInliers = 15;
    if (quality == "Low") m_minInliers = 10;
    
    qDebug() << "=== START REKONSTRUKCJI ===";
    qDebug() << "Jakość:" << quality << "| GPU:" << m_useGpu << "| MVS:" << useMVS;
    
    m_useFastMode = !useMVS;

    QDir imgDir(m_imagesPath);
    QDir workDir(m_workspacePath);

    if (imgDir.canonicalPath() == workDir.canonicalPath()) {
        emit errorOccurred("BŁĄD: Folder wyjściowy nie może być folderem ze zdjęciami.");
        return;
    }

    if (!QDir().mkpath(m_workspacePath)) {
        emit errorOccurred("Nie można utworzyć folderu roboczego: " + m_workspacePath);
        return;
    }
    
    m_isCanceled = false;
    runNextStep();
}

void ColmapReconstructionManager::cancel()
{
    m_isCanceled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "Zatrzymywanie procesu COLMAP...";
#ifdef Q_OS_WIN
        m_process->kill();
#else
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
#endif
    }
    emit errorOccurred("Anulowano przez użytkownika.");
}

void ColmapReconstructionManager::runNextStep() {
    if (m_isCanceled) return;

    m_stepTimer.restart();

    QString colmapBinary = "colmap";
    QStringList args;

    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };
    
    QString useGpuStr = m_useGpu ? "1" : "0";

    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Analiza cech (Feature Extraction)...", 10);
        args << "feature_extractor"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("image_path", m_imagesPath)
             << arg("SiftExtraction.use_gpu", useGpuStr)
             << arg("SiftExtraction.max_image_size", QString::number(m_maxImageSize))
             << arg("SiftExtraction.max_num_features", QString::number(m_maxFeatures));
             
        if (m_numThreads > 0) args << arg("SiftExtraction.num_threads", QString::number(m_numThreads));
        if (!m_useGpu) args << arg("SiftExtraction.domain_size_pooling", "1");
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Dopasowywanie cech (Matching)...", 25);
        args << "exhaustive_matcher"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("SiftMatching.use_gpu", useGpuStr);
             
        if (m_numThreads > 0) args << arg("SiftMatching.num_threads", QString::number(m_numThreads));
        break;

    case 2: // SPARSE RECONSTRUCTION (MAPPER)
        emit progressUpdated("Rekonstrukcja rzadka (Sparse)...", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        args << "mapper"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("image_path", m_imagesPath)
             << arg("output_path", m_workspacePath + "/sparse")
             << arg("Mapper.tri_ignore_two_view_tracks", "0")
             << arg("Mapper.init_min_num_inliers", "10")
             << arg("Mapper.init_min_tri_angle", "4");
        break;

    case 3: // CONVERTER (Eksport punktów)
        if (m_useFastMode) {
            // W trybie Fast Mode eksportujemy tylko rzadką chmurę
            // Zostanie ona przetworzona przez Open3D po zakończeniu tego procesu
            emit progressUpdated("Eksport chmury punktów...", 90);
            args << "model_converter"
                 << arg("input_path", m_workspacePath + "/sparse/0")
                 << arg("output_path", m_workspacePath + "/sparse_cloud.ply") // Zmieniono nazwę na tymczasową
                 << arg("output_type", "PLY");
            
            // UWAGA: Nie ustawiamy tutaj skoku do 6/7. Zrobimy to w obsłudze finished().
        } else {
            // Przygotowanie do MVS
            emit progressUpdated("Przygotowanie do MVS...", 55);
            QDir(m_workspacePath + "/dense").mkpath(".");
            args << "image_undistorter"
                 << arg("image_path", m_imagesPath)
                 << arg("input_path", m_workspacePath + "/sparse/0")
                 << arg("output_path", m_workspacePath + "/dense")
                 << arg("output_type", "COLMAP")
                 << arg("max_image_size", QString::number(m_maxImageSize));
        }
        break;

    case 4: // DENSE STEREO
        emit progressUpdated("Obliczanie głębi (Dense Stereo)...", 70);
        args << "patch_match_stereo"
             << arg("workspace_path", m_workspacePath + "/dense")
             << arg("workspace_format", "COLMAP")
             << arg("PatchMatchStereo.geom_consistency", m_useGeomConsistency ? "true" : "false");
        
        if (!m_useGpu) args << arg("PatchMatchStereo.gpu_index", "-1"); 
        break;

    case 5: // FUSION
        emit progressUpdated("Tworzenie gęstej chmury (Fusion)...", 85);
        args << "stereo_fusion"
             << arg("workspace_path", m_workspacePath + "/dense")                  
             << arg("workspace_format", "COLMAP")
             << arg("input_type", "photometric")
             << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // MESHING (Poisson - Colmap Built-in)
        // Używane tylko w pełnym trybie MVS
        emit progressUpdated("Generowanie siatki (Meshing)...", 90);
        args << "poisson_mesher"
             << arg("input_path", m_workspacePath + "/dense/fused.ply")
             << arg("output_path", m_workspacePath + "/model.ply");
        break;
        
    case 7: // FINAL CONVERSION (Assimp)
        // <--- 2. WYCZYSZCZONO TEN KROK (Tylko konwersja Assimp) ---
        if (!m_useFastMode && m_outputFormat == "OBJ") {
             emit progressUpdated("Konwersja do OBJ...", 95);
             QString inputFile = m_workspacePath + "/model.ply";
             QString outputFile = m_workspacePath + "/model.obj";
             
             if (m_process) m_process->deleteLater();
             m_process = new QProcess(this);
             m_process->start("assimp", QStringList() << "export" << inputFile << outputFile);
             
             connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [=](int exitCode, QProcess::ExitStatus) {
                if (exitCode == 0 && QFile::exists(outputFile)) {
                    emit finished(outputFile);
                } else {
                    emit finished(inputFile);
                }
             });
             return; // Wyjście, bo Assimp działa asynchronicznie
        } else {
             // Jeśli FastMode, to wynik został już zwrócony wcześniej (w Open3D logic).
             // Jeśli FullMode i format PLY:
             QString finalModel = m_workspacePath + "/model.ply";
             if (QFile::exists(finalModel)) emit finished(finalModel);
             else emit errorOccurred("Nie znaleziono pliku wynikowego.");
             return;
        }
        break;

    default:
        return;
    }

    // --- Uruchomienie procesu COLMAP ---
    if (m_process) m_process->deleteLater();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GLOG_logtostderr", "1");
    env.insert("GLOG_stderrthreshold", "0"); 
    m_process->setProcessEnvironment(env);

    qDebug() << "Executing:" << colmapBinary << args.join(" ");

    // Parsowanie wyjścia
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        while (m_process->canReadLine()) {
            QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
            static QRegularExpression reProgress("\\[\\s*(\\d+)/(\\d+)\\s*\\]");
            QRegularExpressionMatch match = reProgress.match(line);
            
            if (match.hasMatch()) {
                int cur = match.captured(1).toInt();
                int tot = match.captured(2).toInt();
                if (tot > 0) {
                    int stepBase = 0;
                    if (m_currentStep == 0) stepBase = 0;
                    else if (m_currentStep == 1) stepBase = 15;
                    else if (m_currentStep == 2) stepBase = 30;
                    else if (m_currentStep == 4) stepBase = 60;
                    else if (m_currentStep == 5) stepBase = 85;
                    
                    int pct = stepBase + (int)((float)cur/tot * 15.0f);
                    emit progressUpdated(line, pct);
                }
            } else if (!line.isEmpty()) {
               if(line.contains("error", Qt::CaseInsensitive)) qDebug() << "COLMAP ERROR:" << line;
            }
        }
    });

    // --- 3. KLUCZOWA ZMIANA: Obsługa zakończenia procesu i Open3D ---
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            
            // JEŚLI TO BYŁ KROK 3 (Konwerter) W TRYBIE FAST MODE -> URUCHOM OPEN3D
            if (m_currentStep == 3 && m_useFastMode) {
                QString inputCloud = m_workspacePath + "/sparse_cloud.ply";
                QString outputMesh = m_workspacePath + "/model.ply"; // Open3D zapisze jako OBJ

                emit progressUpdated("Generowanie Mesha (Open3D)...", 95);
                qDebug() << "Uruchamiam MeshGenerator::plyToMesh...";

                // To jest operacja blokująca, ale Open3D jest szybki na małych chmurach.
                // Można to przenieść do QtConcurrent::run w przyszłości.
                MeshGenerator::MeshParams params;
                bool success = MeshGenerator::plyToMesh(inputCloud, outputMesh, params);

                if (success) {
                    emit finished(outputMesh);
                } else {
                    emit errorOccurred("Nie udało się wygenerować mesha przez Open3D.");
                }
                return; // KOŃCZYMY TUTAJ, nie idziemy do kolejnych kroków
            }
            
            // Standardowa pętla dla trybu pełnego
            if (m_currentStep == 99) m_currentStep = 100;
            else m_currentStep++;
            runNextStep();
        } else {
            QString err = m_process->readAllStandardOutput();
            qDebug() << "COLMAP CRASH LOG:" << err;
            emit errorOccurred("Błąd etapu COLMAP (Kod " + QString::number(exitCode) + ")\n" + err);
        }
    });

    m_process->start(colmapBinary, args);
}
