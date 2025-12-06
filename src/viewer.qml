import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.AssetUtils 

Item {
    id: root
    anchors.fill: parent

    // --- LEWY HUD: INFO ---
    Rectangle {
        id: infoPanel
        z: 1000
        color: "#AA000000"
        width: 320
        height: 240
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        radius: 5
        border.color: view.activeFocus ? "#00FF00" : "#555"
        border.width: 2

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            Text { text: "INFO & KAMERA"; color: "cyan"; font.bold: true }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
            
            Text { text: "Wymiary Oryginału: " + modelDimsString; color: "yellow"; font.pixelSize: 11 }
            Text { text: "Auto-Mnożnik: x" + transformNode.autoScaleFactor.toFixed(2); color: "orange"; font.pixelSize: 11 }
            
            Button {
                text: "CENTRUUJ I NORMALIZUJ"
                Layout.fillWidth: true
                background: Rectangle { color: "#444"; radius: 3 }
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter }
                onClicked: { 
                    calculateAutoFit(); 
                    view.forceActiveFocus(); 
                }
            }
            
            Text { text: "WASD = Latanie | Shift = Szybko"; color: "gray"; font.pixelSize: 10 }
            Text { text: "LPM + Mysz = Rozglądanie"; color: "gray"; font.pixelSize: 10 }
        }
    }

    property string modelDimsString: "-"

    // --- PRAWY HUD: TRANSFORMACJE ---
    Rectangle {
        id: transformPanel
        z: 1000
        color: "#CC000000"
        width: 300
        height: parent.height - 20
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        radius: 5
        border.color: "#00AAFF"
        border.width: 1

        ScrollView {
            anchors.fill: parent
            anchors.margins: 5
            clip: true

            ColumnLayout {
                width: parent.width - 20
                spacing: 8

                Text { text: "EDYCJA (Względem Auto-Skali)"; color: "#00AAFF"; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                
                // --- SKALA RELATYWNA ---
                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
                // Teraz suwak mnoży wynik auto-skalowania. 1.0 = Idealny rozmiar.
                Text { text: "ZOOM OBIEKTU: " + scaleSlider.value.toFixed(2) + "x"; color: "white"; font.bold: true }
                Slider {
                    id: scaleSlider
                    Layout.fillWidth: true
                    from: 0.1; to: 10.0; value: 1.0 
                }

                // --- POZYCJA ---
                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
                Text { text: "PRZESUWANIE"; color: "orange" }
                
                // Zwiększyłem zakresy suwaków pozycji
                Text { text: "X: " + posX.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posX; Layout.fillWidth: true; from: -1000; to: 1000; value: 0 }
                
                Text { text: "Y: " + posY.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posY; Layout.fillWidth: true; from: -1000; to: 1000; value: 0 }
                
                Text { text: "Z: " + posZ.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posZ; Layout.fillWidth: true; from: -1000; to: 1000; value: 0 }

                // --- ROTACJA ---
                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
                Text { text: "OBRACANIE"; color: "lightgreen" }
                
                Text { text: "X: " + rotX.value.toFixed(0) + "°"; color: "red"; font.pixelSize: 10 }
                Slider { id: rotX; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                
                Text { text: "Y: " + rotY.value.toFixed(0) + "°"; color: "green"; font.pixelSize: 10 }
                Slider { id: rotY; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                
                Text { text: "Z: " + rotZ.value.toFixed(0) + "°"; color: "blue"; font.pixelSize: 10 }
                Slider { id: rotZ; Layout.fillWidth: true; from: 0; to: 360; value: 0 }

                Button {
                    text: "RESETUJ SUWAKI"
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    background: Rectangle { color: "#882222"; radius: 3 }
                    contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter }
                    onClicked: {
                        blockUpdates = true;
                        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
                        posX.value = 0; posY.value = 0; posZ.value = 0;
                        scaleSlider.value = 1.0;
                        blockUpdates = false;
                        view.forceActiveFocus();
                    }
                }
            }
        }
    }

    View3D {
        id: view
        anchors.fill: parent
        focus: true

        environment: SceneEnvironment {
            clearColor: "#303030"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            depthPrePassEnabled: true
        }

        PerspectiveCamera { id: camera; z: 200; y: 100; clipNear: 1.0; clipFar: 100000.0 }
        
        DirectionalLight { eulerRotation.x: -45; eulerRotation.y: -45; brightness: 1.5; castsShadow: true }
        DirectionalLight { eulerRotation.x: 45; eulerRotation.y: 45; brightness: 1.2; color: "#FFDEAD" }
        PointLight { position: camera.position; brightness: 2.0; color: "white" }

        AxisHelper { enableXZGrid: true; gridColor: "#555"; scale: Qt.vector3d(200, 200, 200) }

        // --- KONTENER TRANSFORMACJI ---
        Node {
            id: transformNode
            
            // To jest ten magiczny czynnik. Obliczamy go raz przy ładowaniu.
            property real autoScaleFactor: 1.0

            // Wzór na ostateczną skalę: AUTO_SKALA * SUWAK_UŻYTKOWNIKA
            // Dzięki temu suwak zawsze działa w "ludzkim" zakresie (0.1 - 10),
            // a autoScaleFactor odwala brudną robotę (np. x5000 albo x0.001)
            scale: Qt.vector3d(
                autoScaleFactor * scaleSlider.value, 
                autoScaleFactor * scaleSlider.value, 
                autoScaleFactor * scaleSlider.value
            )
            
            position: Qt.vector3d(posX.value, posY.value, posZ.value)
            eulerRotation: Qt.vector3d(rotX.value, rotY.value, rotZ.value)

            RuntimeLoader {
                id: loader
                source: ""
                instancing: null
                
                onStatusChanged: {
                    if (status === RuntimeLoader.Ready) {
                        if (!blockUpdates) calculateAutoFit();
                    }
                }
            }
        }

        WasdController {
            controlledObject: camera
            speed: 5.0; shiftSpeed: 100.0 // Szybszy shift dla wygody
            keysEnabled: true; mouseEnabled: true
        }
    }

    property bool blockUpdates: false

    function calculateAutoFit() {
        var bMin = loader.bounds.minimum;
        var bMax = loader.bounds.maximum;
        var sizeVec = bMax.minus(bMin);
        var maxDim = Math.max(sizeVec.x, Math.max(sizeVec.y, sizeVec.z));

        // Wyświetlanie wymiarów oryginału
        modelDimsString = sizeVec.x.toFixed(5) + " x " + sizeVec.y.toFixed(5);

        if (maxDim <= 0.000001) return; // Zabezpieczenie przed dzieleniem przez 0

        // 1. Centrowanie wewnętrzne (loader przesuwa się wewnątrz transformNode)
        var center = bMin.plus(bMax).times(0.5);
        loader.position = center.times(-1);

        // 2. OBLICZANIE MNOŻNIKA NORMALIZACJI
        // Chcemy, żeby model ZAWSZE miał rozmiar docelowy = 100 jednostek.
        var targetSize = 100.0;
        
        // Ustawiamy właściwość Node'a, a nie suwak!
        transformNode.autoScaleFactor = targetSize / maxDim;

        // 3. Reset suwaka użytkownika do 1.0 (czyli "100% znormalizowanego rozmiaru")
        blockUpdates = true;
        scaleSlider.value = 1.0;
        
        // 4. Ustawienie na podłodze
        // Model ma teraz wysokość = sizeVec.y * autoScaleFactor
        var normalizedHeight = sizeVec.y * transformNode.autoScaleFactor;
        posY.value = normalizedHeight / 2.0;
        
        posX.value = 0; posZ.value = 0;
        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
        blockUpdates = false;

        // 5. Kamera
        camera.position = Qt.vector3d(0, normalizedHeight, 250);
        camera.lookAt(Qt.vector3d(0, normalizedHeight/2.0, 0));
        
        console.log("AutoFit wykonany. MaxDim oryginału:", maxDim, "AutoMnożnik:", transformNode.autoScaleFactor);
    }

    function loadModel(path) {
        blockUpdates = false;
        loader.source = "";
        loader.source = path;
        view.forceActiveFocus();
    }
    
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: (mouse) => { view.forceActiveFocus(); mouse.accepted = false; }
        onWheel: (wheel) => {
            var speed = 5.0;
            camera.position = camera.position.plus(camera.forward.times(wheel.angleDelta.y * 0.01 * speed));
        }
    }
}
