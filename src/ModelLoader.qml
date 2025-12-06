import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils 

Node {
    id: root
    property url source

    RuntimeLoader {
        id: loader
        source: root.source
        
        // Ważne: Wyśrodkowanie modelu (jeśli był przesunięty przy eksporcie)
        instancing: null
        
        onStatusChanged: {
            if (status === RuntimeLoader.Ready) {
                console.log("Loader: Załadowano geometrię.");
                
                // Centrowanie (opcjonalne, ale przydatne)
                loader.position = Qt.vector3d(0, 0, 0);
                
                // Wypisz rozmiar modelu w logach (pomaga w debugowaniu skali)
                console.log("Loader Bounds:", loader.bounds.minimum, loader.bounds.maximum);

            } else if (status === RuntimeLoader.Error) {
                console.log("Loader BŁĄD:", errorString);
            }
        }
    }
}
