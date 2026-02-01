#include "ColmapReconstructionManager.h"
#include "SettingsDialog.h"
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
    // Pobieramy ustawienia. Jeśli SettingsDialog nie ma tej metody, 
    // zmień 'useGpu' na 'false' na sztywno dla Twojego laptopa.
    // W idealnym świecie dodasz checkbox w SettingsDialog.
    bool useMVS = SettingsDialog::isMvsEnabled();
    
    // TODO: Dodaj metodę SettingsDialog::isGpuEnabled() w przyszłości.
    // Na razie bezpiecznik: Jeśli nie ma Nvidii, dajemy false.
    m_useGpu = false; 

    // Opcjonalnie: Prosta autodetekcja (jeśli chcesz automatyki)
    if (QFile::exists("/proc/driver/nvidia/version")) {
         m_useGpu = true;
    }

    m_useFastMode = !useMVS; 

    qDebug() << "=== START REKONSTRUKCJI ===";
    qDebug() << "Tryb MVS (Gęsta chmura):" << useMVS;
    qDebug() << "Akceleracja GPU:" << m_useGpu;

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

    // Helper do argumentów
    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };
    
    // Helper do flag GPU/CPU
    QString useGpuStr = m_useGpu ? "1" : "0";

    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Analiza cech (Feature Extraction)...", 10);
        args << "feature_extractor"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("image_path", m_imagesPath)
             << arg("SiftExtraction.use_gpu", useGpuStr);
        
        // Optymalizacja dla CPU: domain_size_pooling oszczędza pamięć i czas
        if (!m_useGpu) {
             args << arg("SiftExtraction.domain_size_pooling", "1");
             args << arg("SiftExtraction.estimate_affine_shape", "0"); // Szybciej na CPU
        }
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Dopasowywanie cech (Matching)...", 25);
        args << "exhaustive_matcher"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("SiftMatching.use_gpu", useGpuStr);
        break;

    case 2: // SPARSE RECONSTRUCTION (MAPPER)
        emit progressUpdated("Rekonstrukcja rzadka (Sparse)...", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        args << "mapper"
             << arg("database_path", m_workspacePath + "/database.db")
             << arg("image_path", m_imagesPath)
             << arg("output_path", m_workspacePath + "/sparse")
             // Domyślne parametry dla lepszej stabilności
             << arg("Mapper.tri_ignore_two_view_tracks", "0")
             << arg("Mapper.init_min_num_inliers", "15");
        break;

    case 3: // CONVERTER / UNDISTORTER
        if (m_useFastMode) {
            // Szybki export rzadkiej chmury
            emit progressUpdated("Eksport chmury punktów...", 90);
            args << "model_converter"
                 << arg("input_path", m_workspacePath + "/sparse/0")
                 << arg("output_path", m_workspacePath + "/model.ply")
                 << arg("output_type", "PLY");
            m_currentStep = 99; // Skok do końca
        } else {
            // Przygotowanie pod MVS (Dense)
            emit progressUpdated("Przygotowanie do MVS...", 55);
            QDir(m_workspacePath + "/dense").mkpath(".");
            args << "image_undistorter"
                 << arg("image_path", m_imagesPath)
                 << arg("input_path", m_workspacePath + "/sparse/0")
                 << arg("output_path", m_workspacePath + "/dense")
                 << arg("output_type", "COLMAP")
                 << arg("max_image_size", "2000");
        }
        break;

    case 4: // DENSE STEREO (PatchMatch)
        emit progressUpdated("Obliczanie głębi (Dense Stereo)...", 70);
        args << "patch_match_stereo"
             << arg("workspace_path", m_workspacePath + "/dense")
             << arg("workspace_format", "COLMAP")
             << arg("PatchMatchStereo.geom_consistency", "true");
        
        // Jeśli CPU, wyłączamy indeks GPU (COLMAP 3.8+ wspiera CPU dense, ale jest wolne)
        if (!m_useGpu) {
            args << arg("PatchMatchStereo.gpu_index", "-1"); 
        }
        break;

    case 5: // FUSION
        emit progressUpdated("Tworzenie gęstej chmury (Fusion)...", 85);
        args << "stereo_fusion"
             << arg("workspace_path", m_workspacePath + "/dense")                    
             << arg("workspace_format", "COLMAP")
             << arg("input_type", "photometric")
             << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // MESHING (Poisson)
        emit progressUpdated("Generowanie siatki (Meshing)...", 90);
        args << "poisson_mesher"
             << arg("input_path", m_workspacePath + "/dense/fused.ply")
             << arg("output_path", m_workspacePath + "/model.ply");
        break;

    default:
        // KONIEC
        if (QFile::exists(m_workspacePath + "/model.ply")) {
            emit finished(m_workspacePath + "/model.ply");
        } else {
            emit errorOccurred("Proces zakończony, ale nie znaleziono pliku wynikowego model.ply");
        }
        return;
    }

    // --- Uruchomienie procesu (Czysty QProcess) ---
    
    if (m_process) m_process->deleteLater();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // Ustawiamy zmienne środowiskowe, żeby COLMAP nie buforował wyjścia
    // To zastępuje hack z "/usr/bin/script"
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GLOG_logtostderr", "1");
    env.insert("GLOG_stderrthreshold", "0"); 
    m_process->setProcessEnvironment(env);

    qDebug() << "Executing:" << colmapBinary << args.join(" ");

    // Parsowanie wyjścia (Pasek postępu)
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        while (m_process->canReadLine()) {
            QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
            
            // Regex dla Colmapa [ 5/10]
            static QRegularExpression reProgress("\\[\\s*(\\d+)/(\\d+)\\s*\\]");
            QRegularExpressionMatch match = reProgress.match(line);
            
            if (match.hasMatch()) {
                int cur = match.captured(1).toInt();
                int tot = match.captured(2).toInt();
                if (tot > 0) {
                    // Prosta kalkulacja postępu globalnego
                    // (Można to dopracować, ale działa)
                    int stepBase = 0;
                    if (m_currentStep == 0) stepBase = 0;       // Extraction
                    else if (m_currentStep == 1) stepBase = 15; // Matching
                    else if (m_currentStep == 2) stepBase = 30; // Sparse
                    else if (m_currentStep == 4) stepBase = 60; // Dense
                    else if (m_currentStep == 5) stepBase = 85; // Fusion
                    
                    int pct = stepBase + (int)((float)cur/tot * 15.0f); // 15% na krok
                    emit progressUpdated(line, pct);
                }
            } else if (!line.isEmpty()) {
               // Logowanie ważniejszych komunikatów
               if(line.contains("error", Qt::CaseInsensitive)) qDebug() << "COLMAP ERROR:" << line;
            }
        }
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
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
