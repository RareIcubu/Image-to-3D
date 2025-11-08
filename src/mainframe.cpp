// mainframe.cpp

#include "mainframe.h"
#include "ui_mainframe.h"

// Dołącz potrzebne klasy
#include <QFileDialog>
#include <QFileSystemModel>
#include <QStringList>
#include <QGraphicsScene>       // <-- DODAJ
#include <QGraphicsPixmapItem>  // <-- DODAJ
#include <QPixmap>
#include <QWheelEvent>

MainFrame::MainFrame(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::MainFrame)
    , m_dirModel(new QFileSystemModel(this)) // Zainicjuj JEDEN model
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(new QGraphicsPixmapItem())
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


    m_scene->addItem(m_pixmapItem);

    // 2. Powiedz 'graphicsView' z .ui, żeby patrzył na naszą scenę
    ui->graphicsView->setScene(m_scene);

    // 3. (Opcjonalnie) Ustaw tryb przeciągania myszką
    //    Teraz możesz przesuwać obrazek wciśniętą rolką lub lewym przyciskiem
    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView->installEventFilter(this);

    // 4. (Opcjonalnie) Dodaj lepsze renderowanie
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);


    // Połącz sygnał ładowania z naszym slotem
    connect(m_dirModel, &QFileSystemModel::directoryLoaded,
            this, &MainFrame::onModelLoaded);
}

MainFrame::~MainFrame()
{
    delete ui;
}

bool MainFrame::eventFilter(QObject *watched, QEvent *event)
{
    // Sprawdź, czy zdarzenie pochodzi z naszego graphicsView
    if (watched == ui->graphicsView)
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
                ui->graphicsView->scale(scaleFactor, scaleFactor);
            } else {
                // Kręcenie kółkiem w dół (od siebie) - Zoom Out
                ui->graphicsView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            }

            // Zjedliśmy to zdarzenie - nie puszczaj go dalej
            return true;
        }
    }

    // Przekaż wszystkie inne zdarzenia (jak kliknięcia itp.)
    // do domyślnej obsługi
    return QFrame::eventFilter(watched, event);
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

void MainFrame::on_treeView_clicked(const QModelIndex &index)
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
    ui->graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);

}
