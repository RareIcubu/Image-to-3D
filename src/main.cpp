#include <QApplication>
#include <iostream>
#include "mainwindow.h"
#include "Theme.h"

// Dołączamy nasz plik konfiguracyjny z wersją
#include "config.h"

int main(int argc, char *argv[])
{
    // Informacja w konsoli, że program działa
    std::cout << "Uruchamiam ImageTo3D w wersji: " << PROJECT_VERSION << std::endl;

    // Inicjalizacja aplikacji Qt
    QApplication app(argc, argv);

    // Dark Mode
    Theme::applyDarkPalette(app);

    // Stworzenie głównego okna
    MainWindow *mainWindow = new MainWindow();

    // Ustawienie tytułu okna
    mainWindow->setWindowTitle("ImageTo3D Konwerter");

    // Pokaż okno
    mainWindow->show();

    // Uruchom pętlę zdarzeń aplikacji
    return app.exec();
}
