// mainframe.h
#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <QFrame>
#include <QModelIndex>
#include <QEvent>

// --- ZMIANY TUTAJ ---
// Deklaracje zapowiadające
class QFileSystemModel;
class QGraphicsScene;
class QGraphicsPixmapItem;
// (usuń deklaracje proxy, jeśli wróciłeś do QFileSystemModel)

namespace Ui {
class MainFrame;
}

class MainFrame : public QFrame
{
    Q_OBJECT

public:
    explicit MainFrame(QWidget *parent = nullptr);
    ~MainFrame();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();

    // Ten slot zostaje bez zmian
    void on_treeView_clicked(const QModelIndex &index);

private:
    Ui::MainFrame *ui;
    QFileSystemModel *m_dirModel; // Zakładam, że masz jeden model

    // --- NOWE ZMIENNE ---
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem; // Obrazek, który będziemy podmieniać
};

#endif // MAINFRAME_H
