// MainWindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QEvent>

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();

    // Ten slot zostaje bez zmian
    void on_treeView_clicked(const QModelIndex &index);

    void on_actionO_programie_triggered();

private:
    Ui::MainWindow *ui;
    QFileSystemModel *m_dirModel; // Zakładam, że masz jeden model

    // --- NOWE ZMIENNE ---
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem; // Obrazek, który będziemy podmieniać
};

#endif // MainWindow_H
