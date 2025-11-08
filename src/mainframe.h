#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <QFrame>

class QFileSystemModel;

namespace Ui {
class MainFrame;
}

class MainFrame : public QFrame
{
    Q_OBJECT

public:
    explicit MainFrame(QWidget *parent = nullptr);
    ~MainFrame();

private slots:
    void on_DirectoryButton_clicked();
    void onModelLoaded();

private:
    Ui::MainFrame *ui;
    QFileSystemModel *m_dirModel;
};

#endif // MAINFRAME_H
