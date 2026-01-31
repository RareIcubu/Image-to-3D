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

private slots:
    void on_btnSelectPath_clicked();
    void on_buttonBox_accepted();
    void checkGpuAvailability();

private:
    Ui::SettingsDialog *ui;
    void loadSettings();
    void saveSettings();
};

#endif // SETTINGSDIALOG_H
