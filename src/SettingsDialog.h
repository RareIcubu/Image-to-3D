#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    // Static helper to get default output path globally
    static QString getDefaultOutputPath();
    static bool isMvsEnabled();
    static bool isGpuAvailable();
    static QString getQuality(); // "Low", "Medium", "High"
    
    // Zaawansowane
    static int getNumThreads();
    static int getMaxImageSize();
    static int getMaxFeatures();
    
    static bool isGeomConsistencyEnabled();
    static QString getOutputFormat();

private slots:
    void on_btnSelectPath_clicked();
    void on_buttonBox_accepted();
    void on_comboQuality_currentIndexChanged(int index); // Preset logic
    void checkGpuAvailability();

private:
    Ui::SettingsDialog *ui;
    void loadSettings();
    void saveSettings();
};

#endif // SETTINGSDIALOG_H
