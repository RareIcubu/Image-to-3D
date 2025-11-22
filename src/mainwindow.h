// MainWindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QEvent>
#include <QDir>
#include <QThread> // Include QThread
#include "reconstructionmanager.h"


// --- ZMIANY TUTAJ ---
// Deklaracje zapowiadające
class QFileSystemModel;
class QGraphicsScene;
class QGraphicsPixmapItem;
// (usuń deklaracje proxy, jeśli wróciłeś do QFileSystemModel)

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
    // This signal will trigger the worker thread to start
    void requestReconstruction(const QString &imagesPath, const QString &outputPath);


protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();

    // Ten slot zostaje bez zmian
    void on_treeView_clicked(const QModelIndex &index);

    void on_actionO_programie_triggered();


    // Slot for YOUR TEST (Connect this to pushButton_2 in .ui)
    void on_pushButton_2_clicked();

    // Updates from the background manager
    void onProgressUpdated(QString step, int percentage);
    void onReconstructionFinished(QString modelPath);


private:
    Ui::MainWindow *ui;
    QFileSystemModel *m_dirModel; // Zakładam, że masz jeden model

    // The logic handler
    ReconstructionManager *m_manager;
    QThread *m_workerThread; // The thread instance

    // Store the selected path
    QString m_selectedDirectory;

    // --- NOWE ZMIENNE ---
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem; // Obrazek, który będziemy podmieniać
};

#endif // MainWindow_H
