#include "mainwindow.h"
#include "Theme.h"
#include <QAction>
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
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QProcess>
#include <QDirIterator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dirModel(new QFileSystemModel(this))
    , m_manager(new ReconstructionManager())
    , m_workerThread(new QThread(this))
    , m_aiManager(new AIReconstructionManager())
    , m_aiThread(new QThread(this))
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(new QGraphicsPixmapItem())
{
    ui->setupUi(this);

    // Konfiguracja widoku 3D
    setup3DView();

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

    refreshModelList();

    ui->textEdit->setReadOnly(true);
    appendLog("--- Gotowy do pracy ---");

    m_manager->moveToThread(m_workerThread);
    connect(this, &MainWindow::requestReconstruction, m_manager, &ReconstructionManager::startReconstruction);
    connect(m_manager, &ReconstructionManager::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_manager, &ReconstructionManager::finished, this, &MainWindow::onReconstructionFinished);
    connect(m_manager, &ReconstructionManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_workerThread, &QThread::finished, m_manager, &QObject::deleteLater);
    m_workerThread->start();

    m_aiManager->moveToThread(m_aiThread);
    connect(this, &MainWindow::requestAiReconstruction, m_aiManager, &AIReconstructionManager::startAI);
    connect(m_aiManager, &AIReconstructionManager::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_aiManager, &AIReconstructionManager::finished, this, &MainWindow::onReconstructionFinished);
    connect(m_aiManager, &AIReconstructionManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_aiThread, &QThread::finished, m_aiManager, &QObject::deleteLater);
    m_aiThread->start();

    // === DARK MODE ===

    m_actionToggleTheme = new QAction(tr("Włącz tryb jasny"), this);
    m_actionToggleTheme->setCheckable(false);
    connect(m_actionToggleTheme, &QAction::triggered, this, &MainWindow::toggleTheme);

    if (ui->menu_Widok) {
        ui->menu_Widok->addSeparator();
        ui->menu_Widok->addAction(m_actionToggleTheme);
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

void MainWindow::setup3DView()
{
    QQuickWidget *view = ui->view3DWidget;
    QQmlEngine *engine = view->engine();

    // Importy dla środowiska Linux/Docker
    engine->addImportPath("/usr/lib/x86_64-linux-gnu/qt6/qml");

    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl::fromLocalFile("viewer.qml"));

    // --- Obsługa klawiatury (WASD) ---
    view->setFocusPolicy(Qt::StrongFocus);
    view->setAttribute(Qt::WA_Hover);
    view->setFocus();
    
    if (view->status() == QQuickWidget::Error) {
        for (const auto &error : view->errors()) qDebug() << error.toString();
    }
}
// --- DARK MODE ---

void MainWindow::toggleTheme()
{
    // Toggle flag
    m_darkMode = !m_darkMode;

    if (m_darkMode) {
        // switch to dark
        Theme::applyDarkPalette(*qobject_cast<QApplication*>(QApplication::instance()));
        if (m_actionToggleTheme) m_actionToggleTheme->setText(tr("Włącz tryb jasny"));
    } else {
        // switch to light
        Theme::applyLightPalette(*qobject_cast<QApplication*>(QApplication::instance()));
        if (m_actionToggleTheme) m_actionToggleTheme->setText(tr("Włącz tryb ciemny"));
    }

    // Force UI refresh
    qApp->processEvents();
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

    ui->pushButton_2->setEnabled(false);
    ui->textEdit->clear();
    appendLog("--- START PROCESU ---");
    ui->progressBar->setRange(0, 0);
    ui->progressBar->show();

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

void MainWindow::onProgressUpdated(QString msg, int percentage)
{
    if (!msg.trimmed().isEmpty()) appendLog(msg.trimmed());
    if (percentage < 0) return;
    if (ui->progressBar->maximum() == 0) ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(percentage);
}

void MainWindow::onReconstructionFinished(QString modelPath)
{
    ui->progressBar->hide();
    ui->pushButton_2->setEnabled(true);
    appendLog("--- SUKCES ---");
    QMessageBox::information(this, "Gotowe", "Model 3D został utworzony:\n" + modelPath);

    // Automatyczne ładowanie wyniku
    QUrl fileUrl = QUrl::fromLocalFile(modelPath);
    QQuickItem *rootObject = ui->view3DWidget->rootObject();
    if (rootObject) {
         QMetaObject::invokeMethod(rootObject, "loadModel", Q_ARG(QVariant, fileUrl.toString()));
    }
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
    QMessageBox::about(this, "O programie", "ImageTo3D\nWersja: " PROJECT_VERSION);
}

void MainWindow::refreshModelList()
{
    ui->comboBox_4->clear();
    ui->comboBox_4->addItem("Fotogrametria (COLMAP)", "colmap");
    QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
    QDir dir(modelsPath);
    QStringList filters; filters << "*.onnx";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo &fi : files) {
        ui->comboBox_4->addItem("AI: " + fi.fileName(), fi.absoluteFilePath());
    }
}

// --- ŁADOWANIE MODELU Z KONWERSJĄ ASSIMP ---
void MainWindow::on_pushButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open 3D Model"),
                                                    QDir::homePath(),
                                                    tr("3D Files (*.obj *.ply *.fbx *.glb *.gltf *.stl *.dae *.3ds);;All Files (*)"));

    if (fileName.isEmpty()) return;

    QString finalPath = fileName;
    QFileInfo fi(fileName);
    QString ext = fi.suffix().toLower();

    // Konwersja na GLB dla Qt Quick 3D
    if (ext != "glb" && ext != "gltf") {
        appendLog("Konwersja modelu " + ext + " do GLB...");

        QString outputDir = QDir::tempPath() + "/qt_model_conversion";
        QDir().mkpath(outputDir);

        // Timestamp dla unikalności (cache busting)
        QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        QString outputFile = outputDir + "/model_" + timestamp + ".glb";

        QProcess converter;
        // Assimp musi być w PATH
        converter.start("assimp", QStringList() << "export" << fileName << outputFile);

        if (!converter.waitForFinished(30000)) {
            appendLog("Błąd: Timeout konwersji assimp.");
            return;
        }

        if (converter.exitCode() != 0) {
            appendLog("Błąd konwersji: " + converter.readAllStandardError());
        } else {
            if (QFile::exists(outputFile)) {
                finalPath = outputFile;
                appendLog("Konwersja udana: " + finalPath);
            }
        }
    }

    QUrl fileUrl = QUrl::fromLocalFile(finalPath);
    QQuickItem *rootObject = ui->view3DWidget->rootObject();

    if (rootObject) {
        ui->view3DWidget->setFocus();
        QMetaObject::invokeMethod(rootObject, "loadModel", Q_ARG(QVariant, fileUrl.toString()));
    }
}
