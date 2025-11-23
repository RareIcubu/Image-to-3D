#include "reconstructionmanager.h"
#include <QDir>
#include <QCoreApplication>

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

    QDir dir(m_workspacePath);
    if (!dir.exists()) {
        dir.mkpath(".");
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
    QString program = "colmap";
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

    // --- BRANCHING POINT ---

    case 3:
        if (m_useFastMode) {
            // FIX: Instead of Meshing (which requires missing CGAL),
            // we just CONVERT the sparse points to a viewable .PLY file.
            emit progressUpdated("Exporting Point Cloud...\n", 90);

            arguments << "model_converter"
                      << arg("input_path", m_workspacePath + "/sparse/0") // Internal binary format
                      << arg("output_path", m_workspacePath + "/model.ply") // Viewable file
                      << arg("output_type", "PLY");

            // We are done after this
            m_currentStep = 99;
        } else {
            // NORMAL PATH: Prepare for Dense
            emit progressUpdated("Undistorting Images...\n", 55);
            QDir(m_workspacePath + "/dense").mkpath(".");
            arguments << "image_undistorter"
                      << arg("image_path", m_imagesPath)
                      << arg("input_path", m_workspacePath + "/sparse/0")
                      << arg("output_path", m_workspacePath + "/dense")
                      << arg("output_type", "COLMAP")
                      << arg("max_image_size", "2000");
        }
        break;

    case 4: // DENSE RECONSTRUCTION (Skipped if Fast Mode)
        if (m_fallbackToCpu) {
            emit progressUpdated("Calculating Depth Maps (CPU Fallback)...\n", 70);
            arguments << "patch_match_stereo"
                      << arg("workspace_path", m_workspacePath + "/dense")
                      << arg("workspace_format", "COLMAP")
                      << arg("PatchMatchStereo.geom_consistency", "false")
                      << arg("PatchMatchStereo.gpu_index", "");
        } else {
            emit progressUpdated("Calculating Depth Maps (GPU)...\n", 70);
            arguments << "patch_match_stereo"
                      << arg("workspace_path", m_workspacePath + "/dense")
                      << arg("workspace_format", "COLMAP")
                      << arg("PatchMatchStereo.geom_consistency", "true");
        }
        break;

    case 5: // FUSION (Skipped if Fast Mode)
        emit progressUpdated("Fusing Point Cloud...\n", 85);
        arguments << "stereo_fusion"
                  << arg("workspace_path", m_workspacePath + "/dense")
                  << arg("workspace_format", "COLMAP")
                  << arg("input_type", "geometric")
                  << arg("output_path", m_workspacePath + "/dense/fused.ply");
        break;

    case 6: // POISSON MESHING (Skipped if Fast Mode)
        emit progressUpdated("Generating Mesh...\n", 90);
        arguments << "poisson_mesher"
                  << arg("input_path", m_workspacePath + "/dense/fused.ply")
                  << arg("output_path", m_workspacePath + "/model.ply");
        break;

    default:
        // FINISHED
        emit finished(m_workspacePath + "/model.ply");
        QProcess::startDetached("meshlab", QStringList() << m_workspacePath + "/model.ply");
        return;
    }

    if (m_process) {
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus status) {
                if (exitCode == 0) {
                    qDebug() << "Step" << m_currentStep << "finished successfully.";

                    if (m_currentStep == 99) {
                        // If we just finished Delaunay (Step 99), we are done.
                        m_currentStep = 100; // Triggers 'default' next time
                    } else {
                        m_currentStep++;
                    }
                    runNextStep();
                } else {
                    qDebug() << "Step" << m_currentStep << "failed with code" << exitCode;

                    // Step 4 Fallback Logic (Only relevant if NOT in fast mode)
                    if (m_currentStep == 4 && !m_fallbackToCpu && !m_useFastMode) {
                        qDebug() << "Dense reconstruction failed. Switching to CPU mode...";
                        m_fallbackToCpu = true;
                        runNextStep();
                        return;
                    }

                    QString errorOutput = m_process->readAllStandardOutput();
                    qDebug() << errorOutput;
                    emit errorOccurred("Step " + QString::number(m_currentStep) + " failed.\n\nLog:\n" + errorOutput.right(500));
                }
            });

    qDebug() << "Running:" << program << arguments.join(" ");
    m_process->start(program, arguments);
}
