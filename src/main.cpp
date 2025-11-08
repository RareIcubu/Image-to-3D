#include <QApplication>
#include <QMainWindow>
#include <iostream>
#include "mainframe.h"

// Dołączamy nasz plik konfiguracyjny z wersją
#include "config.h"

int main(int argc, char *argv[])
{
    // Informacja w konsoli, że program działa
    std::cout << "Uruchamiam ImageTo3D w wersji: " << PROJECT_VERSION << std::endl;

    // Inicjalizacja aplikacji Qt
    QApplication app(argc, argv);

    // Stworzenie głównego okna
    QMainWindow mainWindow;

    // Ustawienie tytułu okna
    mainWindow.setWindowTitle("ImageTo3D Konwerter");

    // Ustawienie domyślnego rozmiaru okna
    mainWindow.resize(800, 600);

    MainFrame* mainF = new MainFrame(&mainWindow);
    mainWindow.setCentralWidget(mainF);

    // Pokaż okno
    mainWindow.show();

    // Uruchom pętlę zdarzeń aplikacji
    return app.exec();
}
