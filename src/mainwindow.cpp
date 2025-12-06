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
#include <QVBoxLayout> // Required for embedding

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dirModel(new QFileSystemModel(this))
    // Inicjalizacja managerów
    , m_manager(new ReconstructionManager())
    , m_workerThread(new QThread(this))
    , m_aiManager(new AIReconstructionManager())
    , m_aiThread(new QThread(this))
    // Grafika
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(new QGraphicsPixmapItem())
{
    ui->setupUi(this);
    
    // === KONFIGURACJA GUI ===
    if (ui->menu_Widok) {
        ui->menu_Widok->addAction(ui->inputDockWidget->toggleViewAction());
        ui->menu_Widok->addAction(ui->logDockWidget->toggleViewAction());
    }

    m_dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    m_dirModel->setNameFilters(QStringList() << "*.jpg" << "*.jpeg" << "*.png");
    m_dirModel->setNameFilterDisables(false);
    ui->treeView->setModel(m_dirModel);
    for (int i = 1; i < 4; ++i) ui->treeView->hideColumn(i);
    ui->treeView->setRootIndex(m_dirModel->setRootPath("/app"));
    
    connect(m_dirModel, &QFileSystemModel::directoryLoaded, this, &MainWindow::onModelLoaded);

    m_scene->addItem(m_pixmapItem);
    ui->graphicsView_3->setScene(m_scene);
    ui->graphicsView_3->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView_3->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView_3->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    ui->graphicsView_3->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView_3->installEventFilter(this);

    // Wypełnienie listy modeli (TERAZ TO ZADZIAŁA, BO FUNKCJA JEST NA KOŃCU PLIKU)
    refreshModelList();

    ui->textEdit->setReadOnly(true);
    appendLog("--- Gotowy do pracy ---");

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
    connect(m_aiManager, &AIReconstructionManager::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_aiManager, &AIReconstructionManager::finished, this, &MainWindow::onReconstructionFinished);
    connect(m_aiManager, &AIReconstructionManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_aiThread, &QThread::finished, m_aiManager, &QObject::deleteLater);
    m_aiThread->start();

    // --- 1. Setup 3D Viewer in Existing Tab ---

    // Create the viewer
    // We set 'this' as parent initially, layout will handle reparenting
    m_modelViewer = new ModelViewer(this);

    // We target the widget inside the tab, usually named 'tab_3d_page' (or similar in your UI)
    QWidget *targetTab = ui->tab_3d_page;

    if (targetTab) {
        // FIX for "Attempting to add QLayout" error:
        // Check if the tab already has a layout (created in Designer)
        if (targetTab->layout() != nullptr) {
            // If layout exists, use it. This makes the viewer expand to fill the tab.
            targetTab->layout()->addWidget(m_modelViewer);
        } else {
            // If no layout exists, create a new one
            QVBoxLayout *layout = new QVBoxLayout(targetTab);
            layout->setContentsMargins(0, 0, 0, 0); // Remove margins for full immersion
            layout->addWidget(m_modelViewer);
        }
    } else {
        // Fallback: If we couldn't find the tab by name
        qDebug() << "Could not find 'tab_3d_page' in UI. Adding new tab instead.";
        ui->tabWidget->addTab(m_modelViewer, "3D View");
    }
}

MainWindow::~MainWindow()
{
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

    // Logika wyboru
    QString selection = ui->comboBox_4->currentData().toString();

    if (selection == "colmap") {
        appendLog("Metoda: Fotogrametria");
        QString folderName = imgDir.dirName();
        imgDir.cdUp();
        QString outputDir = imgDir.filePath(folderName + "_workspace");
        emit requestReconstruction(m_selectedDirectory, outputDir);
    } else {
        appendLog("Metoda: AI (" + QFileInfo(selection).fileName() + ")");
        emit requestAiReconstruction(m_selectedDirectory, selection);
    }
}

// --- AKTUALIZACJA STANU ---

void MainWindow::onProgressUpdated(QString msg, int percentage)
{
    if (!msg.trimmed().isEmpty()) {
        appendLog(msg.trimmed());
    }

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
    m_modelViewer->loadModel(modelPath);

    if (ui->tab_3d_page) {
        ui->tabWidget->setCurrentWidget(ui->tab_3d_page);
    }
}

void MainWindow::onErrorOccurred(QString message)
{
    ui->progressBar->hide();
    ui->pushButton_2->setEnabled(true);
    appendLog("!!! BŁĄD: " + message);
    QMessageBox::critical(this, "Błąd", message);
}

// --- TEJ FUNKCJI BRAKOWAŁO ---
void MainWindow::on_actionO_programie_triggered()
{
    QMessageBox::about(this, "O programie", 
        "ImageTo3D\nProjekt studencki: Grafika i GUI\nWersja: " PROJECT_VERSION);
}

// --- TEJ TEŻ BRAKOWAŁO ---
void MainWindow::refreshModelList()
{
    ui->comboBox_4->clear();
    
    // Opcja 1: COLMAP (Zawsze)
    ui->comboBox_4->addItem("Fotogrametria (COLMAP)", "colmap");

    // Opcja 2+: Modele AI z folderu
    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    QDir dir(modelsPath);
    QStringList filters; filters << "*.onnx";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty()) {
        appendLog("Info: Brak modeli ONNX w " + modelsPath);
    }

    for (const QFileInfo &fi : files) {
        // Tekst: "AI: [Nazwa]", Data: Pełna ścieżka
        ui->comboBox_4->addItem("AI: " + fi.fileName(), fi.absoluteFilePath());
    }
}

// NOTE: This function name 'on_showModel_clicked' uses Qt's auto-connection naming convention.
// If you see "No matching signal for on_showModel_clicked", it means you don't have a button
// named "showModel" in your .ui file, or you renamed it.
// You can ignore the warning if you call this manually, or rename the button in Qt Designer.
void MainWindow::on_showModel_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Open 3D Model",
                                                    m_selectedDirectory.isEmpty() ? QDir::homePath() : m_selectedDirectory,
                                                    "3D Files (*.obj *.ply *.fbx *.stl)");

    if (fileName.isEmpty()) return;

    m_modelViewer->loadModel(fileName);

    // Switch to the tab containing the viewer
    if (ui->tab_3d_page) {
        ui->tabWidget->setCurrentWidget(ui->tab_3d_page);
    }
}
