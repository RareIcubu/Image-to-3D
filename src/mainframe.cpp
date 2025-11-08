// mainframe.cpp

#include "mainframe.h"
#include "ui_mainframe.h"

// Dołącz potrzebne klasy
#include <QFileDialog>
#include <QFileSystemModel>
#include <QStringList>

// Plik .h powinien teraz zawierać tylko:
// class QFileSystemModel;
// ...
// QFileSystemModel *m_dirModel;


MainFrame::MainFrame(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::MainFrame)
    , m_dirModel(new QFileSystemModel(this)) // Zainicjuj JEDEN model
{
    ui->setupUi(this);

    // --- Ustawienia modelu ---

    // 1. Ustaw filtr QDir (co ma wczytywać z dysku)
    m_dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);

    // 2. Ustaw filtr nazw (ignoruje wielkość liter)
    m_dirModel->setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");

    // 3. TO JEST KLUCZ:
    // Mówi modelowi, aby UKRYWAŁ pliki, a nie je "wyszarzał".
    m_dirModel->setNameFilterDisables(false);

    // --- Ustawienia widoku ---
    ui->treeView->setModel(m_dirModel); // Podłącz model do widoku

    // Ukryj niepotrzebne kolumny
    ui->treeView->hideColumn(1); // Rozmiar
    ui->treeView->hideColumn(2); // Typ
    ui->treeView->hideColumn(3); // Data

    ui->progressBar->hide();
    ui->progressBar->setRange(0, 0); // Ustaw na "spinner"

    // 4. TO JEST DRUGI KLUCZ:
    // Ustaw ścieżkę startową na folder, w którym JEST KOD (/app),
    // a nie na pusty folder domowy (/home/devuser).
    ui->treeView->setRootIndex(m_dirModel->setRootPath("/app"));

    // Połącz sygnał ładowania z naszym slotem
    connect(m_dirModel, &QFileSystemModel::directoryLoaded,
            this, &MainFrame::onModelLoaded);
}

MainFrame::~MainFrame()
{
    delete ui;
}

void MainFrame::on_DirectoryButton_clicked()
{
    // Użyj ostatnio wybranej ścieżki jako startowej, zamiast homePath
    QString startPath = m_dirModel->rootPath();

    QString dirPath = QFileDialog::getExistingDirectory(this,
                                                        tr("Wybierz folder"),
                                                        startPath);
    if (dirPath.isEmpty()) {
        return;
    }

    ui->progressBar->show();
    ui->treeView->setRootIndex(m_dirModel->setRootPath(dirPath));
}

void MainFrame::onModelLoaded()
{
    ui->progressBar->hide();
}
