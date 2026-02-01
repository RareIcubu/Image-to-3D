#include <QtTest>
#include "ColmapReconstructionManager.h"
#include "SettingsDialog.h"
#include "PointCloudGeometry.h"
#include <QTemporaryFile>

class Test_ColmapLogic : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        qDebug() << "Rozpoczynam testy jednostkowe ImageTo3D...";
    }

    void testManagerCreation() {
        ColmapReconstructionManager manager;
        QVERIFY(true); // Jeśli tu dotarło, to konstruktor działa
    }
    
    void testPointCloudParsing() {
        // Tworzymy tymczasowy plik PLY
        QTemporaryFile plyFile;
        if (plyFile.open()) {
            QTextStream out(&plyFile);
            out << "ply\n";
            out << "format ascii 1.0\n";
            out << "element vertex 3\n";
            out << "property float x\n";
            out << "property float y\n";
            out << "property float z\n";
            out << "property uchar red\n";
            out << "property uchar green\n";
            out << "property uchar blue\n";
            out << "end_header\n";
            out << "0 0 0 255 0 0\n";
            out << "10 10 10 0 255 0\n";
            out << "20 20 20 0 0 255\n";
            out.flush();
            
            PointCloudGeometry geometry;
            QUrl url = QUrl::fromLocalFile(plyFile.fileName());
            geometry.setSource(url);
            
            // Sprawdzamy czy geometria coś wczytała
            // PointCloudGeometry oblicza bounds w updateData()
            
            // Bounds w QQuick3DGeometry nie są łatwo dostępne z API C++,
            // więc sprawdzamy naszą metodę pomocniczą.
            // Oczekujemy 3 wierzchołków (Pure Points)
            QVERIFY(geometry.vertexCount() == 3); 
            
            // W idealnym świecie PointCloudGeometry wystawiłoby metodę vertexCount() do testów
            // Ale sam fakt, że setSource nie crashuje na poprawnym pliku to już coś.
        }
    }
};

QTEST_MAIN(Test_ColmapLogic)
#include "Test_ColmapLogic.moc"
