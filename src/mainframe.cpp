#include "mainframe.h"
#include "ui_mainframe.h"

#include <QFileDialog>
#include <QFileSystemModel>

MainFrame::MainFrame(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::MainFrame)
    , m_dirModel(new QFileSystemModel(this))
{
    ui->setupUi(this);
    // Ustawiamy filtry (nie chcemy widzieć "." i "..")
    m_dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);

    // Podłączamy nasz model do widoku (treeView) z pliku .ui
    ui->treeView->setModel(m_dirModel);

    // Możemy też ukryć kolumny, których nie potrzebujemy (np. Rozmiar, Data modyfikacji)
    ui->treeView->hideColumn(1); // Ukryj kolumnę Rozmiar
    ui->treeView->hideColumn(2); // Ukryj kolumnę Typ
    ui->treeView->hideColumn(3); // Ukryj kolumnę Data

    // Ustawiamy ścieżkę startową (np. folder domowy)
    ui->treeView->setRootIndex(m_dirModel->setRootPath(QDir::homePath()));

    // Nie musimy pisać connect(...), ponieważ Qt automatycznie połączy
    // sygnał clicked() z przycisku 'DirectoryButton'
    // z naszą funkcją 'on_DirectoryButton_clicked()'
}

MainFrame::~MainFrame()
{
    delete ui;
}

// --- Implementacja naszego slotu ---
void MainFrame::on_DirectoryButton_clicked()
{
    // 1. Otwórz standardowe okno dialogowe do wyboru folderu
    QString dirPath = QFileDialog::getExistingDirectory(this,
                                                        tr("Wybierz folder"),
                                                        QDir::homePath()); // Zacznij od folderu domowego

    // 2. Sprawdź, czy użytkownik coś wybrał (a nie kliknął "Anuluj")
    if (dirPath.isEmpty()) {
        return;
    }

    // 3. Ustaw nową ścieżkę jako główny widok w treeView
    // Model sam zajmie się resztą (załadowaniem plików itp.)
    ui->treeView->setRootIndex(m_dirModel->setRootPath(dirPath));
}
