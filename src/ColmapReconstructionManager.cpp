#include "ColmapReconstructionManager.h"
#include "SettingsDialog.h"
#include "SystemChecks.h"
#include "MeshGenerator.h" // Używamy tego do FastMode i konwersji końcowej
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QDebug>

// Open3D Headers (Potrzebne do konwersji PLY -> OBJ na końcu)
#include <open3d/geometry/TriangleMesh.h>
#include <open3d/io/TriangleMeshIO.h>

ColmapReconstructionManager::ColmapReconstructionManager(QObject *parent) 
    : IReconstructionManager(parent), m_process(nullptr) 
{
}

void ColmapReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    m_imagesPath = imagesPath;
    m_workspacePath = outputDir;
    m_currentStep = 0;
    
    // --- KONFIGURACJA ---
    bool useMVS = SettingsDialog::isMvsEnabled();
    QString quality = SettingsDialog::getQuality();
    
    m_useGpu = SystemChecks::checkCudaAvailable();

    m_maxImageSize = SettingsDialog::getMaxImageSize();
    m_numThreads = SettingsDialog::getNumThreads();
    m_maxFeatures = SettingsDialog::getMaxFeatures();
    m_useGeomConsistency = SettingsDialog::isGeomConsistencyEnabled();
    m_outputFormat = "OBJ"; 
    
    m_minInliers = 15;
    if (quality == "Low") m_minInliers = 10;
    
    qDebug() << "=== START REKONSTRUKCJI (HYBRID MODE) ===";
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
            // W trybie Fast Mode eksportujemy rzadką chmurę jako PLY
            emit progressUpdated("Eksport chmury punktów...", 90);
            args << "model_converter"
                 << arg("input_path", m_workspacePath + "/sparse/0")
                 << arg("output_path", m_workspacePath + "/sparse_cloud.ply") 
                 << arg("output_type", "PLY");
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

    case 6: // MESHING (HYBRID: COLMAP robi Mesha -> Open3D robi konwersję)
        {
            // Używamy natywnego Poisson Mesher z COLMAP
            // Dlaczego? Bo obsługuje parametr "trim" (przycinanie bąbli)
            emit progressUpdated("Generowanie siatki (COLMAP Native)...", 90);
            
            // Zapisujemy do pliku TYMCZASOWEGO .ply
            // (Open3D potem zamieni go na .obj)
            QString tempPly = m_workspacePath + "/temp_model.ply"; 
            
            args << "poisson_mesher"
                 << arg("input_path", m_workspacePath + "/dense/fused.ply")
                 << arg("output_path", tempPly)
                 << arg("PoissonMeshing.trim", "10"); // <--- KLUCZOWE: Usuwa "balony" (wartość > 0)
        }
        break;
        
    case 7: 
        return; // Koniec (obsłużone w finished)

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

    // Parsowanie Logów
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
                    if (m_currentStep == 1) stepBase = 15;
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

    // --- FINISH HANDLER (Logika Hybrydowa) ---
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            
            // --- SCENARIUSZ 1: FAST MODE (Sparse Cloud) ---
            if (m_currentStep == 3 && m_useFastMode) {
                QString inputCloud = m_workspacePath + "/sparse_cloud.ply";
                QString outputMesh = m_workspacePath + "/model.obj";

                emit progressUpdated("Generowanie Mesha (Open3D Fast)...", 95);
                
                MeshGenerator::MeshParams params;
                params.depth = 8; 
                // W FastMode używamy wbudowanego generatora, bo nie mamy dense cloud
                bool success = MeshGenerator::plyToMesh(inputCloud, outputMesh, params);

                if (success) emit finished(outputMesh);
                else emit errorOccurred("Nie udało się wygenerować mesha w trybie Fast.");
                return;
            }
            
            // --- SCENARIUSZ 2: HYBRID MODE (MVS / High Quality) ---
            // Krok 6 (COLMAP Meshing) zakończony sukcesem -> Konwersja na OBJ
            if (m_currentStep == 6) {
                QString tempPly = m_workspacePath + "/temp_model.ply";
                QString finalObj = m_workspacePath + "/model.obj";

                if (QFile::exists(tempPly)) {
                    emit progressUpdated("Konwersja PLY -> OBJ (Open3D)...", 98);
                    qDebug() << "[Manager] Converting COLMAP result to OBJ for Viewer compatibility...";
                    
                    // Używamy Open3D jako konwertera
                    auto mesh = std::make_shared<open3d::geometry::TriangleMesh>();
                    
                    if (open3d::io::ReadTriangleMesh(tempPly.toStdString(), *mesh)) {
                        // Zapisz jako ASCII OBJ (write_ascii=true)
                        bool ok = open3d::io::WriteTriangleMesh(finalObj.toStdString(), *mesh, true, false);
                        
                        if (ok) {
                            // Usuń tymczasowy plik PLY, żeby nie śmiecić
                            QFile::remove(tempPly);
                            emit finished(finalObj);
                        } else {
                            emit errorOccurred("Błąd zapisu pliku OBJ.");
                        }
                    } else {
                        emit errorOccurred("Błąd odczytu tymczasowego pliku PLY.");
                    }
                } else {
                    emit errorOccurred("COLMAP nie utworzył pliku wynikowego.");
                }
                return; // KONIEC PROCESU
            }

            // Normalna pętla (idziemy do następnego kroku)
            m_currentStep++;
            runNextStep();

        } else {
            QString err = m_process->readAllStandardOutput();
            qDebug() << "COLMAP CRASH LOG:" << err;
            emit errorOccurred("Błąd etapu COLMAP (Kod " + QString::number(exitCode) + ")\n" + err);
        }
    });

    m_process->start(colmapBinary, args);
}
