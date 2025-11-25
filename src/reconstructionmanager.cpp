#include "reconstructionmanager.h"
#include <QDir>
#include <QCoreApplication>
#include <QFile> // Dodane brakujące include
#include <QRegularExpression>

ReconstructionManager::ReconstructionManager(QObject *parent) : QObject(parent) {
    m_process = nullptr;
    m_fallbackToCpu = false;
    // Set this to true to use the alternative "Delaunay" pipeline (CPU friendly)
    m_useFastMode = true;
}

void ReconstructionManager::startReconstruction(const QString &imagesPath, const QString &outputDir) {
    m_imagesPath = imagesPath;
    m_workspacePath = outputDir;
    m_currentStep = 0;
    m_fallbackToCpu = false;

    QDir imgDir(m_imagesPath);
    QDir workDir(m_workspacePath);

    // Sprawdzamy, czy ścieżki kanoniczne (rozwiązane skróty/symlinki) są różne
    if (imgDir.canonicalPath() == workDir.canonicalPath()) {
        emit errorOccurred("BŁĄD KRYTYCZNY: Folder wyjściowy nie może być ten sam co folder ze zdjęciami!");
        return;
    }

    // --- CZYSZCZENIE (Opcja 1 - Zalecana: Usuń cały workspace) ---
    if (workDir.exists()) {
        qDebug() << "Wykryto stary workspace. Usuwanie...";
        
        // removeRecursively() usuwa folder wraz z zawartością
        if (!workDir.removeRecursively()) {
            emit errorOccurred("Nie udało się usunąć starego workspace'u. Może COLMAP wciąż działa w tle?");
            return;
        }
    }
    
    /* // --- ALTERNATYWA (Opcja 2: Usuń tylko bazę) ---
    // Użyj tego JEŚLI chcesz zachować inne pliki w tym folderze
    if (workDir.exists()) {
         QFile::remove(m_workspacePath + "/database.db");
         QFile::remove(m_workspacePath + "/database.db-shm");
         QFile::remove(m_workspacePath + "/database.db-wal");
         // Usuń stare foldery z danymi
         QDir(m_workspacePath + "/sparse").removeRecursively();
         QDir(m_workspacePath + "/dense").removeRecursively();
         qDebug() << "Wyczyszczono bazę danych i stare wyniki.";
    }
    */

    // --- TWORZENIE NA NOWO ---
    if (!QDir().mkpath(m_workspacePath)) {
        emit errorOccurred("Nie udało się stworzyć folderu: " + m_workspacePath);
        return;
    }

    qDebug() << "Starting reconstruction in:" << m_workspacePath;
    if (m_useFastMode) {
        qDebug() << "Mode: FAST (Delaunay Meshing, Skips Dense Reconstruction)";
    } else {
        qDebug() << "Mode: HIGH QUALITY (Dense Reconstruction)";
    }

    runNextStep();
}

void ReconstructionManager::runNextStep() {
    m_stepTimer.restart();
    
    // Używamy pełnej ścieżki do COLMAP dla pewności w Dockerze
    QString program = "/usr/bin/colmap";
    QStringList arguments;

    auto arg = [](const QString &key, const QString &val) {
        return "--" + key + "=" + val;
    };

    switch (m_currentStep) {
    case 0: // FEATURE EXTRACTION
        emit progressUpdated("Extracting Features...\n", 10);
        arguments << "feature_extractor"
                  << arg("database_path", m_workspacePath + "/database.db")
                  << arg("image_path", m_imagesPath);
        break;

    case 1: // FEATURE MATCHING
        emit progressUpdated("Matching Features...\n", 25);
        arguments << "exhaustive_matcher"
                  << arg("database_path", m_workspacePath + "/database.db");
        break;

    case 2: // SPARSE RECONSTRUCTION
        emit progressUpdated("Sparse Reconstruction...\n", 40);
        QDir(m_workspacePath + "/sparse").mkpath(".");
        arguments << "mapper"
                  << arg("database_path", m_workspacePath + "/database.db")
                  << arg("image_path", m_imagesPath)
                  << arg("output_path", m_workspacePath + "/sparse");
        break;

    case 3: // MODEL CONVERTER / UNDISTORTER
        if (m_useFastMode) {
            // Tryb Szybki: Konwertuj rzadką chmurę od razu do PLY i zakończ
            emit progressUpdated("Exporting Point Cloud...\n", 90);
            arguments << "model_converter"
                      << arg("input_path", m_workspacePath + "/sparse/0")
                      << arg("output_path", m_workspacePath + "/model.ply")
                      << arg("output_type", "PLY");
            m_currentStep = 99; // Skocz do końca po tym kroku
        } else {
            // Tryb Pełny: Przygotuj zdjęcia do gęstej rekonstrukcji
            emit progressUpdated("Undistorting Images (Preparing Dense)...\n", 55);
            QDir(m_workspacePath + "/dense").mkpath(".");
            arguments << "image_undistorter"
                      << arg("image_path", m_imagesPath)
                      << arg("input_path", m_workspacePath + "/sparse/0")
                      << arg("output_path", m_workspacePath + "/dense")
                      << arg("output_type", "COLMAP")
                      << arg("max_image_size", "2000");
        }
        break;

    case 4: // DENSE RECONSTRUCTION (Tylko w trybie pełnym)
        if (m_fallbackToCpu) {
            emit progressUpdated("Calculating Depth Maps (CPU Fallback)...\n", 70);
            arguments << "patch_match_stereo"
                      << arg("workspace_path", m_workspacePath + "/dense")
                      << arg("workspace_format", "COLMAP")
                      << arg("PatchMatchStereo.geom_consistency", "false")
                      << arg("PatchMatchStereo.gpu_index", "-1"); // Wymuś brak GPU
        } else {
            emit progressUpdated("Calculating Depth Maps (GPU)...\n", 70);
            arguments << "patch_match_stereo"
                      << arg("workspace_path", m_workspacePath + "/dense")
                      << arg("workspace_format", "COLMAP")
                      << arg("PatchMatchStereo.geom_consistency", "true");
        }
        break;

    case 5: // FUSION (Tylko w trybie pełnym)
        emit progressUpdated("Fusing Point Cloud...\n", 85);
        arguments << "stereo_fusion"
                  << arg("workspace_path", m_workspacePath + "/dense")
                  << arg("workspace_format", "COLMAP")
                  << arg("input_type", "geometric")
                  << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // POISSON MESHING (Tylko w trybie pełnym)
        emit progressUpdated("Generating Mesh...\n", 90);
        arguments << "poisson_mesher"
                  << arg("input_path", m_workspacePath + "/dense/fused.ply")
                  << arg("output_path", m_workspacePath + "/model.ply");
        break;

    default:
        // Koniec procesu - sprawdź czy plik istnieje
        if (QFile::exists(m_workspacePath + "/model.ply")) {
            emit finished(m_workspacePath + "/model.ply");
        } else {
            emit errorOccurred("Proces zakończony sukcesem, ale brak pliku wynikowego model.ply");
        }
        return;
    }

    if (m_process) {
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // --- CZYTANIE LOGÓW I OBLICZANIE % ---
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray data = m_process->readAllStandardOutput();
        QString output = QString::fromLocal8Bit(data).trimmed();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            int progressValue = -1;

            // Szukamy wzorca [1/50] w logach COLMAP
            static QRegularExpression reProgress("\\[(\\d+)/(\\d+)\\]");
            QRegularExpressionMatch match = reProgress.match(line);

            if (match.hasMatch()) {
                int current = match.captured(1).toInt();
                int total = match.captured(2).toInt();
                if (total > 0) {
                    float stepPercent = (float)current / total;
                    
                    // Mapowanie lokalnego postępu na pasek globalny (0-100%)
                    int base = 0; 
                    int range = 0;

                    if (m_currentStep == 0)      { base = 0;  range = 15; } // Features
                    else if (m_currentStep == 1) { base = 15; range = 15; } // Matching
                    else if (m_currentStep == 2) { base = 30; range = 25; } // Sparse
                    else if (m_currentStep == 3) { base = 55; range = 5;  } // Undistort
                    else if (m_currentStep == 4) { base = 60; range = 25; } // Dense Stereo (Długie!)
                    else if (m_currentStep == 5) { base = 85; range = 10; } // Fusion
                    else if (m_currentStep == 6) { base = 95; range = 5;  } // Meshing
                    
                    if (range > 0) {
                        progressValue = base + (int)(stepPercent * range);
                    }
                }
            }
            emit progressUpdated(line, progressValue);
        }
    });

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus status) {
                if (exitCode == 0) {
                    // Sukces
                    qint64 duration = m_stepTimer.elapsed();
                    QString timeString = QString::number(duration / 1000.0, 'f', 2) + "s";
                    qDebug() << "Step" << m_currentStep << "finished successfully.";

                    emit progressUpdated(QString("--- Krok %1 zakończony w %2 ---\n").arg(m_currentStep).arg(timeString), -1);

                    if (m_currentStep == 99) {
                        m_currentStep = 100; // Koniec (Fast Mode)
                    } else {
                        m_currentStep++; // Następny krok
                    }
                    runNextStep();
                } else {
                    // Błąd
                    qDebug() << "Step" << m_currentStep << "failed with code" << exitCode;

                    // Automatyczne przełączenie na CPU w kroku 4 (Dense Stereo)
                    if (m_currentStep == 4 && !m_fallbackToCpu && !m_useFastMode) {
                        qDebug() << "Dense reconstruction failed (GPU error?). Switching to CPU mode...";
                        emit progressUpdated("!!! Błąd GPU. Przełączanie na CPU... !!!\n", -1);
                        m_fallbackToCpu = true;
                        runNextStep(); // Spróbuj ponownie ten sam krok
                        return;
                    }

                    QString errorOutput = m_process->readAllStandardOutput(); // Doczytaj resztę błędu
                    emit errorOccurred("Step " + QString::number(m_currentStep) + " failed code " + QString::number(exitCode) + "\n" + errorOutput.right(300));
                }
            });

    qDebug() << "Executing:" << program << arguments.join(" ");
    
    // Uruchamiamy BEZPOŚREDNIO (To jest najstabilniejsza opcja w Dockerze)
    m_process->start(program, arguments);
}
