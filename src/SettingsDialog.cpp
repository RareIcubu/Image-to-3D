#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
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
    // Try to run nvidia-smi
    QProcess process;
    process.start("nvidia-smi", QStringList() << "-L");
    process.waitForFinished(1000);
    
    bool hasGpu = (process.exitCode() == 0);
    
    if (hasGpu) {
        ui->lblGpuStatus->setText("Wykryto GPU NVIDIA: Dostępne");
        ui->lblGpuStatus->setStyleSheet("color: green; font-weight: bold;");
        ui->chkEnableMVS->setEnabled(true);
        ui->chkEnableMVS->setToolTip("Włącza gęstą rekonstrukcję (wymaga GPU)");
    } else {
        ui->lblGpuStatus->setText("Brak GPU NVIDIA (lub sterowników w kontenerze)");
        ui->lblGpuStatus->setStyleSheet("color: red; font-weight: bold;");
        ui->chkEnableMVS->setChecked(false);
        ui->chkEnableMVS->setEnabled(false);
        ui->chkEnableMVS->setToolTip("Wymaga karty NVIDIA");
    }
}

void SettingsDialog::loadSettings()
{
    QSettings settings("ImageTo3D", "Config");
    QString defaultPath = settings.value("output/defaultPath", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    
    bool useMVS = settings.value("colmap/useMVS", true).toBool();
    
    ui->lineEditPath->setText(defaultPath);
    
    // Only allow MVS checked if GPU is available
    if (ui->chkEnableMVS->isEnabled()) {
        ui->chkEnableMVS->setChecked(useMVS);
    }
}

void SettingsDialog::saveSettings()
{
    QSettings settings("ImageTo3D", "Config");
    settings.setValue("output/defaultPath", ui->lineEditPath->text());
    settings.setValue("colmap/useMVS", ui->chkEnableMVS->isChecked());
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
    // Check GPU again just in case settings were manipulated
    if (!isGpuAvailable()) return false;
    return settings.value("colmap/useMVS", true).toBool();
}

bool SettingsDialog::isGpuAvailable()
{
     // Quick check without re-running process every time? 
     // For safety, let's assume if nvidia-smi works, it works.
     // But strictly speaking, we can cache this.
     static bool checked = false;
     static bool available = false;
     
     if (!checked) {
        QProcess process;
        process.start("nvidia-smi", QStringList() << "-L");
        process.waitForFinished(1000);
        available = (process.exitCode() == 0);
        checked = true;
     }
     return available;
}
