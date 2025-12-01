#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "config.h" 

#include <QFileDialog>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dirModel(new QFileSystemModel(this))
    // Inicjalizacja managerów (bez rodzica, bo idą do wątków)
    , m_manager(new ReconstructionManager())
    , m_workerThread(new QThread(this))
    , m_aiManager(new AIReconstructionManager())
    , m_aiThread(new QThread(this))
    // Grafika
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(new QGraphicsPixmapItem())
{
    ui->setupUi(this);
    refreshModelList();
    // === KONFIGURACJA WĄTKU 1: COLMAP ===
    m_manager->moveToThread(m_workerThread);
    connect(this, &MainWindow::requestReconstruction, m_manager, &ReconstructionManager::startReconstruction);
    connect(m_manager, &ReconstructionManager::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_manager, &ReconstructionManager::finished, this, &MainWindow::onReconstructionFinished);
    connect(m_manager, &ReconstructionManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_workerThread, &QThread::finished, m_manager, &QObject::deleteLater);
    m_workerThread->start();

    // === KONFIGURACJA WĄTKU 2: AI (ONNX) ===
    m_aiManager->moveToThread(m_aiThread);
    connect(this, &MainWindow::requestAiReconstruction, m_aiManager, &AIReconstructionManager::startAI);
    // Używamy tych samych slotów do GUI, bo interfejs jest wspólny
    connect(m_aiManager, &AIReconstructionManager::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_aiManager, &AIReconstructionManager::finished, this, &MainWindow::onReconstructionFinished);
    connect(m_aiManager, &AIReconstructionManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_aiThread, &QThread::finished, m_aiManager, &QObject::deleteLater);
    m_aiThread->start();

    // === KONFIGURACJA GUI ===
    
    // Docki w menu
    if (ui->menu_Widok) {
        ui->menu_Widok->addAction(ui->inputDockWidget->toggleViewAction());
        ui->menu_Widok->addAction(ui->logDockWidget->toggleViewAction());
    }

    // Drzewo plików
    m_dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    m_dirModel->setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    m_dirModel->setNameFilterDisables(false);
    ui->treeView->setModel(m_dirModel);
    for (int i = 1; i < 4; ++i) ui->treeView->hideColumn(i);
    ui->treeView->setRootIndex(m_dirModel->setRootPath("/app"));
    connect(m_dirModel, &QFileSystemModel::directoryLoaded, this, &MainWindow::onModelLoaded);

    // Graphics View
    m_scene->addItem(m_pixmapItem);
    ui->graphicsView_3->setScene(m_scene);
    ui->graphicsView_3->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView_3->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView_3->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView_3->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView_3->installEventFilter(this);

    // Logi
    ui->textEdit->setReadOnly(true);
    appendLog("--- Gotowy do pracy ---");
}

MainWindow::~MainWindow()
{
    // Bezpieczne zamykanie wątków
    m_workerThread->quit();
    m_workerThread->wait();
    m_aiThread->quit();
    m_aiThread->wait();
    delete ui;
}

// --- FUNKCJE POMOCNICZE ---

void MainWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss] ");
    ui->textEdit->append(timestamp + message);
    // Auto-scroll na dół
    QTextCursor c = ui->textEdit->textCursor();
    c.movePosition(QTextCursor::End);
    ui->textEdit->setTextCursor(c);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->graphicsView_3 && event->type() == QEvent::Wheel) {
        QWheelEvent *we = static_cast<QWheelEvent*>(event);
        double scale = (we->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        ui->graphicsView_3->scale(scale, scale);
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

// --- SLOTY GUI ---

void MainWindow::on_DirectoryButton_clicked()
{
    QString startPath = m_dirModel->rootPath();
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Wybierz folder"), startPath);
    if (!dirPath.isEmpty()) {
        m_selectedDirectory = dirPath;
        ui->progressBar->show();
        ui->treeView->setRootIndex(m_dirModel->setRootPath(dirPath));
        appendLog("Wybrano folder: " + dirPath);
    }
}

void MainWindow::onModelLoaded() { ui->progressBar->hide(); }

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    if (m_dirModel->isDir(index)) return;
    QString filePath = m_dirModel->filePath(index);
    QPixmap pixmap(filePath);
    if (!pixmap.isNull()) {
        m_pixmapItem->setPixmap(pixmap);
        ui->graphicsView_3->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
    }
}

// --- START (KLUCZOWA LOGIKA) ---

void MainWindow::on_pushButton_2_clicked()
{
    if (m_selectedDirectory.isEmpty()) {
        QMessageBox::warning(this, "Błąd", "Najpierw wybierz folder ze zdjęciami!");
        return;
    }

    // Sprawdź czy są zdjęcia
    QDir imgDir(m_selectedDirectory);
    if (imgDir.entryList(QStringList() << "*.jpg" << "*.png", QDir::Files).isEmpty()) {
        QMessageBox::warning(this, "Brak zdjęć", "W wybranym folderze nie ma plików graficznych.");
        return;
    }

    // Blokada GUI
    ui->pushButton_2->setEnabled(false);
    ui->textEdit->clear();
    appendLog("--- START PROCESU ---");
    ui->progressBar->setRange(0, 0); // Spinner
    ui->progressBar->show();

    // Sprawdzamy wybór metody (comboBox_4 z Twojego UI)
    // Index 0: Fotogrametria, Index 1: AI
   QString selection = ui->comboBox_4->currentData().toString();

    if (selection == "colmap") {
        appendLog("Metoda: Fotogrametria");
        QString folderName = imgDir.dirName();
        imgDir.cdUp();
        QString outputDir = imgDir.filePath(folderName + "_workspace");
        emit requestReconstruction(m_selectedDirectory, outputDir);
    } else {
        // To jest ścieżka do modelu
        appendLog("Metoda: AI (" + QFileInfo(selection).fileName() + ")");
        // Przekazujemy ścieżkę modelu do workera
        emit requestAiReconstruction(m_selectedDirectory, selection);
    }
}

// --- AKTUALIZACJA STANU ---

void MainWindow::onProgressUpdated(QString msg, int percentage)
{
    // Logi
    if (!msg.trimmed().isEmpty()) {
        appendLog(msg.trimmed());
    }

    // Pasek postępu
    if (percentage < 0) return; // -1 = tylko tekst

    if (ui->progressBar->maximum() == 0) {
        ui->progressBar->setRange(0, 100); // Przełącz na tryb procentowy
    }
    ui->progressBar->setValue(percentage);
}

void MainWindow::onReconstructionFinished(QString modelPath)
{
    ui->progressBar->hide();
    ui->pushButton_2->setEnabled(true);
    appendLog("--- SUKCES ---");
    QMessageBox::information(this, "Gotowe", "Model 3D został utworzony:\n" + modelPath);
}

void MainWindow::onErrorOccurred(QString message)
{
    ui->progressBar->hide();
    ui->pushButton_2->setEnabled(true);
    appendLog("!!! BŁĄD: " + message);
    QMessageBox::critical(this, "Błąd", message);
}

void MainWindow::on_actionO_programie_triggered()
{
    QMessageBox::about(this, "O programie", 
        "ImageTo3D\nProjekt studencki: Grafika i GUI\nWersja: " PROJECT_VERSION);
}
void MainWindow::refreshModelList()
{
    // comboBox_4 = Wybór metody (Fotogrametria / AI)
    ui->comboBox_4->clear();
    
    // 1. Opcja stała
    ui->comboBox_4->addItem("Fotogrametria (COLMAP)", "colmap");

    // 2. Skanowanie folderu z modelami
    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    QDir dir(modelsPath);
    QStringList filters; filters << "*.onnx";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty()) {
        appendLog("Info: Brak modeli ONNX w " + modelsPath);
    }

    for (const QFileInfo &fi : files) {
        // Tekst: AI (nazwa pliku), Data: pełna ścieżka do modelu
        ui->comboBox_4->addItem("AI: " + fi.fileName(), fi.absoluteFilePath());
    }
}
