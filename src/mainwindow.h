#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QEvent>
#include <QDir>
#include <QThread>
#include <QQuickWidget>

#include "ColmapReconstructionManager.h"
#include "OnnxReconstructionManager.h"

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
    void startColmap(const QString &imagesPath, const QString &outputPath);
    void startOnnx(const QString &imagesPath, const QString &outputPath);
    void requestCancel(); // New signal to cancel processing

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();
    void on_treeView_clicked(const QModelIndex &index);
    void on_actionO_programie_triggered();
    void on_actionUstawienia_triggered(); // Settings slot
    void on_actionOpenModel_triggered();  // New manual load slot
    void refreshModelList(); 
    void on_pushButton_2_clicked(); // START/STOP button

    // Common slots
    void onProgressUpdated(QString step, int percentage);
    void onReconstructionFinished(QString modelPath);
    void onErrorOccurred(QString message);

    void appendLog(const QString &message);
    // REMOVED: void on_pushButton_clicked(); 

    void toggleTheme();

private:
    Ui::MainWindow *ui;
    void setup3DView();
    void updateStatusLabel(); // Helper to update config label
    
    void resetUiState(); // Helper to reset button state

    QFileSystemModel *m_dirModel;
    QString m_selectedDirectory;

    ColmapReconstructionManager *m_colmapManager;
    QThread *m_workerThread;

    OnnxReconstructionManager *m_onnxManager;
    QThread *m_aiThread;

    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;

    bool m_darkMode = true;
    bool m_isProcessing = false; // Flag to track state
    QAction *m_actionToggleTheme = nullptr;
    QAction *m_actionSettings = nullptr; // Action for settings
};

#endif // MAINWINDOW_H