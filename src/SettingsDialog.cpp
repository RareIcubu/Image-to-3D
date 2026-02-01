#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "SystemChecks.h"
#include <QFileDialog>
#include <QProcess>
#include <QDebug>
#include <QStandardPaths>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    checkGpuAvailability();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::checkGpuAvailability()
{
    bool hasGpu = SystemChecks::checkCudaAvailable();
    
    if (hasGpu) {
        ui->lblGpuStatus->setText("Wykryto GPU NVIDIA: Dostępne");
        ui->lblGpuStatus->setStyleSheet("color: green; font-weight: bold;");
        ui->chkEnableMVS->setEnabled(true);
        ui->chkEnableMVS->setToolTip("Włącza gęstą rekonstrukcję (wymaga GPU)");
    } else {
        ui->lblGpuStatus->setText("Brak GPU NVIDIA (Tryb CPU)");
        ui->lblGpuStatus->setStyleSheet("color: orange; font-weight: bold;");
        
        // FIX: Fizyczne zablokowanie MVS, żeby user nie mógł tego włączyć
        ui->chkEnableMVS->setChecked(false);
        ui->chkEnableMVS->setEnabled(false); 
        ui->chkEnableMVS->setToolTip("Wymaga karty NVIDIA (Opcja niedostępna)");
    }
}

void SettingsDialog::loadSettings()
{
    QSettings settings("ImageTo3D", "Config");
    QString defaultPath = settings.value("output/defaultPath", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    
    bool useMVS = settings.value("colmap/useMVS", false).toBool();
    QString quality = settings.value("colmap/quality", "Medium").toString();

    ui->lineEditPath->setText(defaultPath);
    
    if (ui->chkEnableMVS->isEnabled()) {
        ui->chkEnableMVS->setChecked(useMVS);
    }

    // Zaawansowane
    ui->spinThreads->setValue(settings.value("colmap/numThreads", -1).toInt());
    ui->spinMaxImageSize->setValue(settings.value("colmap/maxImageSize", 2000).toInt());
    ui->spinMaxFeatures->setValue(settings.value("colmap/maxFeatures", 8192).toInt());
    
    ui->chkGeomConsistency->setChecked(settings.value("colmap/geomConsistency", true).toBool());
    
    QString fmt = settings.value("colmap/outputFormat", "PLY").toString();
    int fmtIdx = ui->comboOutputFormat->findData(fmt);
    if (fmtIdx >= 0) ui->comboOutputFormat->setCurrentIndex(fmtIdx);

    // Ustaw combo na końcu, żeby nie triggerować zmiany wartości (chyba że chcemy)
    // Blokujemy sygnały na chwilę
    ui->comboQuality->blockSignals(true);
    if (quality == "Low") ui->comboQuality->setCurrentIndex(0);
    else if (quality == "High") ui->comboQuality->setCurrentIndex(2);
    else ui->comboQuality->setCurrentIndex(1);
    ui->comboQuality->blockSignals(false);
}

void SettingsDialog::saveSettings()
{
    QSettings settings("ImageTo3D", "Config");
    settings.setValue("output/defaultPath", ui->lineEditPath->text());
    settings.setValue("colmap/useMVS", ui->chkEnableMVS->isChecked());
    
    QString qual = "Medium";
    int idx = ui->comboQuality->currentIndex();
    if (idx == 0) qual = "Low";
    else if (idx == 2) qual = "High";
    settings.setValue("colmap/quality", qual);

    // Zaawansowane
    settings.setValue("colmap/numThreads", ui->spinThreads->value());
    settings.setValue("colmap/maxImageSize", ui->spinMaxImageSize->value());
    settings.setValue("colmap/maxFeatures", ui->spinMaxFeatures->value());
    settings.setValue("colmap/geomConsistency", ui->chkGeomConsistency->isChecked());
    
    settings.setValue("colmap/outputFormat", ui->comboOutputFormat->currentData().toString());
}

void SettingsDialog::on_comboQuality_currentIndexChanged(int index)
{
    // PRESET LOGIC: Użytkownik widzi, co się zmienia
    if (index == 0) { // LOW
        ui->spinMaxImageSize->setValue(1000);
        ui->spinMaxFeatures->setValue(4096);
    } else if (index == 1) { // MEDIUM
        ui->spinMaxImageSize->setValue(2000);
        ui->spinMaxFeatures->setValue(8192);
    } else if (index == 2) { // HIGH
        ui->spinMaxImageSize->setValue(4000);
        ui->spinMaxFeatures->setValue(16384);
    }
}

void SettingsDialog::on_btnSelectPath_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Wybierz folder domyślny", ui->lineEditPath->text());
    if (!dir.isEmpty()) {
        ui->lineEditPath->setText(dir);
    }
}

void SettingsDialog::on_buttonBox_accepted()
{
    saveSettings();
    accept();
}

// Static helpers
QString SettingsDialog::getDefaultOutputPath()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("output/defaultPath", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
}

bool SettingsDialog::isMvsEnabled()
{
    QSettings settings("ImageTo3D", "Config");
    // If GPU not available, force false
    if (!SystemChecks::checkCudaAvailable()) return false;
    return settings.value("colmap/useMVS", true).toBool();
}

QString SettingsDialog::getQuality()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/quality", "Medium").toString();
}

int SettingsDialog::getNumThreads()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/numThreads", -1).toInt();
}

int SettingsDialog::getMaxImageSize()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/maxImageSize", 2000).toInt();
}

int SettingsDialog::getMaxFeatures()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/maxFeatures", 8192).toInt();
}

bool SettingsDialog::isGeomConsistencyEnabled()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/geomConsistency", true).toBool();
}

QString SettingsDialog::getOutputFormat()
{
    QSettings settings("ImageTo3D", "Config");
    return settings.value("colmap/outputFormat", "PLY").toString();
}

bool SettingsDialog::isGpuAvailable()
{
     return SystemChecks::checkCudaAvailable();
}
