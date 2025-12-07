#include <QApplication>
#include <iostream>
#include "mainwindow.h"
#include "Theme.h"

// Dołączamy nasz plik konfiguracyjny z wersją
#include "config.h"

int main(int argc, char *argv[])
{
    // 1. Wymuszenie backendu (dla AMD/Docker)
    qputenv("QSG_RHI_BACKEND", "opengl");
    qputenv("QT_QUICK_BACKEND", "rhi");

    // 2. --- KLUCZOWE: Dodanie ścieżki systemowej do pluginów ---
    // Bez tego w Dockerze QQuickWidget gubi wtyczki Assimp
    qputenv("QT_PLUGIN_PATH", "/usr/lib/x86_64-linux-gnu/qt6/plugins");
    
    std::cout << "Uruchamiam ImageTo3D w wersji: " << PROJECT_VERSION << std::endl;

    QApplication app(argc, argv);

    // 3. Dublujemy to w instancji aplikacji dla pewności
    app.addLibraryPath("/usr/lib/x86_64-linux-gnu/qt6/plugins");
    // Dark Mode
    Theme::applyDarkPalette(app);

    // Stworzenie głównego okna
    MainWindow *mainWindow = new MainWindow();

    // Diagnostyka: Wypiszmy, gdzie szukamy
    std::cout << "Szukam pluginów w:" << std::endl;
    for (const QString &path : app.libraryPaths()) {
        std::cout << " - " << path.toStdString() << std::endl;
    }

    mainWindow->setWindowTitle("ImageTo3D Konwerter");
    mainWindow->show();
    // --- DIAGNOSTYKA IMPORTERÓW ---
    std::cout << "\n=== DOSTĘPNE IMPORTERY 3D ===" << std::endl;
    // To wymaga nagłówka: #include <QQuick3DInstancing> lub podobnego, 
    // ale prościej sprawdzić to przez QPluginLoader w pętli, 
    // jednak najprościej zobaczyć to w logach DEBUG z QML.
    
    // Zamiast kodu C++, wymuś zmienną środowiskową, która każe Assimpowi "gadać":
    qputenv("QT_QUICK3D_ASSETIMPORT_DEBUG", "1");
    // -------------------------------
    return app.exec();
}
