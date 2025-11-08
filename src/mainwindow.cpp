#include "mainwindow.h"

#include "ui_mainwindow.h"

// Dołącz potrzebne klasy
#include <QFileDialog>
#include <QFileSystemModel>
#include <QStringList>
#include <QGraphicsScene>       // <-- DODAJ
#include <QGraphicsPixmapItem>  // <-- DODAJ
#include <QPixmap>
#include <QWheelEvent>
#include <QMessageBox>

#include "config.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dirModel(new QFileSystemModel(this)) // Zainicjuj JEDEN model
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(new QGraphicsPixmapItem())
{
    ui->setupUi(this);

    ui->menu_Widok->addAction(ui->inputDockWidget->toggleViewAction());
    ui->menu_Widok->addAction(ui->logDockWidget->toggleViewAction());

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


    m_scene->addItem(m_pixmapItem);

    // 2. Powiedz 'graphicsView_3' z .ui, żeby patrzył na naszą scenę
    ui->graphicsView_3->setScene(m_scene);

    // 3. (Opcjonalnie) Ustaw tryb przeciągania myszką
    //    Teraz możesz przesuwać obrazek wciśniętą rolką lub lewym przyciskiem
    ui->graphicsView_3->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView_3->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView_3->installEventFilter(this);

    // 4. (Opcjonalnie) Dodaj lepsze renderowanie
    ui->graphicsView_3->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView_3->setRenderHint(QPainter::SmoothPixmapTransform);


    // Połącz sygnał ładowania z naszym slotem
    connect(m_dirModel, &QFileSystemModel::directoryLoaded,
            this, &MainWindow::onModelLoaded);

}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Sprawdź, czy zdarzenie pochodzi z naszego graphicsView_3
    if (watched == ui->graphicsView_3)
    {
        // Sprawdź, czy zdarzenie to kółko myszy
        if (event->type() == QEvent::Wheel)
        {
            // Rzutuj zdarzenie na QWheelEvent
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

            // Ustaw współczynnik zoomu
            double scaleFactor = 1.15;

            if (wheelEvent->angleDelta().y() > 0) {
                // Kręcenie kółkiem w górę (do siebie) - Zoom In
                ui->graphicsView_3->scale(scaleFactor, scaleFactor);
            } else {
                // Kręcenie kółkiem w dół (od siebie) - Zoom Out
                ui->graphicsView_3->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            }

            // Zjedliśmy to zdarzenie - nie puszczaj go dalej
            return true;
        }
    }

    // Przekaż wszystkie inne zdarzenia (jak kliknięcia itp.)
    // do domyślnej obsługi
    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::on_DirectoryButton_clicked()
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

void MainWindow::onModelLoaded()
{
    ui->progressBar->hide();
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    // (Jeśli masz proxy, tu musi być mapowanie na sourceIndex)

    // 1. Sprawdź, czy to plik, a nie folder
    if (m_dirModel->isDir(index)) {
        // Czyść stary obrazek, jeśli kliknięto na folder
        m_pixmapItem->setPixmap(QPixmap());
        m_scene->setSceneRect(m_scene->itemsBoundingRect()); // Zresetuj widok
        return;
    }

    // 2. Pobierz pełną ścieżkę do pliku z modelu
    QString filePath = m_dirModel->filePath(index);

    // 3. Wczytaj obraz do obiektu QPixmap
    QPixmap pixmap(filePath);

    // 4. Sprawdź, czy obraz wczytał się poprawnie
    if (pixmap.isNull()) {
        m_pixmapItem->setPixmap(QPixmap()); // Czyść w razie błędu
        m_scene->setSceneRect(m_scene->itemsBoundingRect());
        return;
    }

    // 5. Zamiast ustawiać QLabel, podmień obrazek w naszym QGraphicsPixmapItem
    m_pixmapItem->setPixmap(pixmap);

    // 6. KLUCZOWY KROK: Powiedz widokowi, żeby automatycznie
    //    dopasował zoom i pokazał cały obrazek
    ui->graphicsView_3->fitInView(m_pixmapItem, Qt::KeepAspectRatio);

}

void MainWindow::on_actionO_programie_triggered()
{
    // Używamy standardowego okna dialogowego "About"
    QMessageBox::about(this, // Rodzic (to okno)
                       tr("O programie ImageTo3D"), // Tytuł okna
                       tr("<h3>ImageTo3D Konwerter</h3>" // Tekst (można używać HTML)
                          "<p>Projekt na przedmiot Grafika i GUI.</p>"
                          "<p><b>Wersja:</b> %1</p>" // Użyjemy %1 jako "placeholder"
                          "<p>Autorzy: Jakub Jasiński, Kamil Pojedynek, Kacper Ulanowski</p>")
                           .arg(PROJECT_VERSION) // Wstaw wersję z config.h w miejsce %1
                       );
}

