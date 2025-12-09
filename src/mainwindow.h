#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QEvent>
#include <QDir>
#include <QThread>
#include <QQuickWidget>
#include <QSet>

// --- INCLUDE NASZYCH SILNIKÓW ---
#include "reconstructionmanager.h"
#include "ai_reconstructionmanager.h" // Nowa klasa ONNX Runtime

class QFileSystemModel;
class QGraphicsScene;
class QGraphicsPixmapItem;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    // Sygnał dla COLMAP (wymaga ścieżki wyjściowej, bo tworzymy ją w GUI)
    void requestReconstruction(const QString &imagesPath, const QString &outputPath);
    
    // Sygnał dla AI (AI samo tworzy swój folder roboczy "_ai_workspace")
    void requestAiReconstruction(const QString &imagesPath, const QString &modelPath);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();
    void on_treeView_clicked(const QModelIndex &index);
    void on_actionO_programie_triggered();
    void refreshModelList(); 
    // Przycisk START
    void on_pushButton_2_clicked();

    // Wspólne sloty aktualizacji
    void onProgressUpdated(QString step, int percentage);
    void onReconstructionFinished(QString modelPath);
    void onErrorOccurred(QString message);

    // Funkcja pomocnicza do logowania
    void appendLog(const QString &message);
    // Wybranie i odpalenie podgladu modelu 3d z pliku .obj
    void on_pushButton_clicked();

    // Dark Mode
    void toggleTheme();

    // The slot that triggers when you click the menu item
    void on_actionExportLogs_triggered();

private:
    Ui::MainWindow *ui;
    void setup3DView(); // Helper function
    
    QFileSystemModel *m_dirModel;
    QString m_selectedDirectory;

    // Silnik 1: COLMAP
    ReconstructionManager *m_manager;
    QThread *m_workerThread;

    // Silnik 2: AI (ONNX Runtime)
    AIReconstructionManager *m_aiManager;
    QThread *m_aiThread;

    // Grafika 2D
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;

    // Dark Mode
    bool m_darkMode = true;
    QAction *m_actionToggleTheme = nullptr;

    // This string will hold all your log messages
    QString m_logBuffer;

    // This variable will store your unique list of images
    QSet<QString> m_selectedImages; // <--- ADD THIS

    QDir createAndGetSubDir(const QDir &parentDir);
    void createSymlinksForFiles(const QSet<QString> &sourceFiles, const QDir &targetDir);
};

#endif // MAINWINDOW_H
