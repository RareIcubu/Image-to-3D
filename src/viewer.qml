import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import QtQuick3D.Helpers
import QtQuick3D.AssetUtils
import ImageTo3D 1.0

Item {
    id: root
    anchors.fill: parent

    property bool fixBackFace: false 
    property string currentSource: ""

    // --- DETEKCJA TYPU PLIKU ---
    property bool isMesh: {
        let src = currentSource.toString().toLowerCase();
        return src.endsWith(".obj") || src.endsWith(".glb") || src.endsWith("model.ply");
    }

    property bool isCloud: {
        let src = currentSource.toString().toLowerCase();
        return src.endsWith(".ply") && !src.endsWith("model.ply");
    }

    // --- LEWY HUD ---
    Rectangle {
        id: infoPanel
        z: 1000
        color: "#AA000000"
        width: 320
        height: 300
        anchors.bottom: parent.bottom
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

            Text { 
                text: "Typ: " + (root.isMesh ? "MESH" : (root.isCloud ? "CHMURA" : "BRAK"))
                color: root.isMesh ? "lightgreen" : "yellow"
                font.pixelSize: 12
                font.bold: true
            }
            
            // DIAGNOSTYKA SKALI
            Text { 
                text: "Oryginalny Rozmiar: " + debugSizeString
                color: "white" 
                font.pixelSize: 10 
            }
            Text { 
                text: "Auto-Mnożnik: x" + transformNode.autoScaleFactor.toFixed(2)
                color: "orange" 
                font.pixelSize: 11
                font.bold: true
            }
            
            Button {
                text: "CENTRUUJ I NORMALIZUJ (Auto-Fit)"
                Layout.fillWidth: true
                background: Rectangle { color: "#007ACC"; radius: 3 }
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; font.bold: true }
                onClicked: { 
                    // Wymuś przeliczenie
                    if (root.isMesh && loader.bounds) calculateAutoFit(loader.bounds);
                    else if (root.isCloud && plyModel.bounds) calculateAutoFit(plyModel.bounds);
                    else console.log("Brak bounds do obliczeń!");
                    
                    view.forceActiveFocus(); 
                }
            }
            
            CheckBox {
                text: "Tryb 2D/AI (Odwróć Z)"
                checked: root.fixBackFace
                onCheckedChanged: {
                    root.fixBackFace = checked;
                    view.forceActiveFocus();
                }
                contentItem: Text { text: parent.text; color: "lightgreen"; font.bold: true; leftPadding: 30; verticalAlignment: Text.AlignVCenter }
            }

            Text { text: "WASD = Latanie | Shift = Szybko"; color: "gray"; font.pixelSize: 10 }
            Text { text: "LPM + Mysz = Rozglądanie"; color: "gray"; font.pixelSize: 10 }
        }
    }

    // --- PRAWY HUD ---
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
            anchors.fill: parent; anchors.margins: 5; clip: true
            ColumnLayout {
                width: parent.width - 20; spacing: 8
                Text { text: "EDYCJA RĘCZNA"; color: "#00AAFF"; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                
                Text { text: "ZOOM: " + scaleSlider.value.toFixed(2) + "x"; color: "white"; font.bold: true }
                Slider { id: scaleSlider; Layout.fillWidth: true; from: 0.01; to: 5.0; value: 1.0 } // Zmieniono zakres dla precyzji
                
                Text { text: "PRZESUWANIE"; color: "orange" }
                Slider { id: posX; Layout.fillWidth: true; from: -500; to: 500; value: 0 }
                Slider { id: posY; Layout.fillWidth: true; from: -500; to: 500; value: 0 }
                Slider { id: posZ; Layout.fillWidth: true; from: -500; to: 500; value: 0 }

                Text { text: "OBRACANIE"; color: "lightgreen" }
                Slider { id: rotX; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                Slider { id: rotY; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                Slider { id: rotZ; Layout.fillWidth: true; from: 0; to: 360; value: 0 }

                Button {
                    text: "RESETUJ WIDOK"
                    Layout.fillWidth: true; Layout.topMargin: 10
                    background: Rectangle { color: "#882222"; radius: 3 }
                    contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter }
                    onClicked: {
                        blockUpdates = true;
                        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
                        posX.value = 0; posY.value = 0; posZ.value = 0;
                        scaleSlider.value = 1.0;
                        blockUpdates = false;
                        camera.position = Qt.vector3d(0, 0, 400);
                        camera.lookAt(Qt.vector3d(0, 0, 0));
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
            clearColor: "#202020"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        // Kamera startowa - trochę dalej
        PerspectiveCamera { id: camera; z: 400; y: 0; clipNear: 1.0; clipFar: 20000.0 }
        
        DirectionalLight { eulerRotation.x: -30; eulerRotation.y: -30; brightness: 1.2; castsShadow: false }
        DirectionalLight { eulerRotation.x: 30; eulerRotation.y: 30; brightness: 1.0; color: "#FFDEAD" }
        PointLight { position: camera.position; brightness: 0.8; color: "white" }

        AxisHelper { enableXZGrid: true; gridColor: "#444"; scale: Qt.vector3d(2,2,2) } 

        Node {
            id: transformNode
            property real autoScaleFactor: 1.0

            scale: Qt.vector3d(
                autoScaleFactor * scaleSlider.value, 
                autoScaleFactor * scaleSlider.value, 
                autoScaleFactor * scaleSlider.value * (root.fixBackFace ? -1 : 1)
            )
            position: Qt.vector3d(posX.value, posY.value, posZ.value)
            eulerRotation: Qt.vector3d(rotX.value, rotY.value, rotZ.value)

            // MESH
            RuntimeLoader {
                id: loader
                source: root.isMesh ? root.currentSource : ""
                visible: root.isMesh
                
                onStatusChanged: {
                    if (status === RuntimeLoader.Ready && root.isMesh) {
                        // Mały delay, czasem bounds nie są gotowe w tej samej klatce
                        fitTimer.start();
                    }
                }
            }

            // CLOUD
            Model {
                id: plyModel
                visible: root.isCloud
                geometry: PointCloudGeometry {
                    source: root.isCloud ? root.currentSource : ""
                }
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    pointSize: 5.0
                    baseColor: "white"
                }
                onBoundsChanged: {
                     if (root.isCloud && !blockUpdates) fitTimer.start();
                }
            }
        }

        WasdController { controlledObject: camera; speed: 10.0; shiftSpeed: 200.0 }
    }

    // Timer do opóźnionego dopasowania (Stabilność)
    Timer {
        id: fitTimer
        interval: 100
        repeat: false
        onTriggered: {
            if (root.isMesh) calculateAutoFit(loader.bounds);
            else if (root.isCloud) calculateAutoFit(plyModel.bounds);
        }
    }

    property bool blockUpdates: false
    property string debugSizeString: "-"

    function calculateAutoFit(bounds) {
        if (!bounds) return;
        
        var bMin = bounds.minimum;
        var bMax = bounds.maximum;
        
        // Ignoruj puste bounds (0,0,0)
        if (bMin.length() === 0 && bMax.length() === 0) return;

        var sizeVec = bMax.minus(bMin);
        var maxDim = Math.max(sizeVec.x, Math.max(sizeVec.y, sizeVec.z));
        
        debugSizeString = maxDim.toFixed(4);
        console.log("QML AutoFit: Size=" + maxDim);

        if (maxDim <= 0.000001) return;

        // --- KLUCZOWA ZMIANA: Docelowy rozmiar ---
        var targetSize = 300.0; // Było 100, teraz 300 (większy obiekt)
        var factor = targetSize / maxDim;
        
        transformNode.autoScaleFactor = factor;
        console.log("QML AutoFit: Applied Factor=" + factor);

        // Centrowanie
        var center = bMin.plus(bMax).times(0.5);
        if (root.isMesh) loader.position = center.times(-1);
        else plyModel.position = center.times(-1);

        // Reset suwaków
        blockUpdates = true;
        scaleSlider.value = 1.0;
        posX.value = 0; posY.value = 0; posZ.value = 0;
        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
        blockUpdates = false;
        
        // Ustawienie kamery idealnie na wprost
        camera.position = Qt.vector3d(0, 0, 500); // Odsunięcie na 500
        camera.lookAt(Qt.vector3d(0, 0, 0));
    }

    function loadModel(path) {
        console.log("QML loadModel: " + path);
        currentSource = path;
        view.forceActiveFocus();
    }
    
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: (mouse) => { view.forceActiveFocus(); mouse.accepted = false; }
        onWheel: (wheel) => {
            camera.position = camera.position.plus(camera.forward.times(wheel.angleDelta.y * 0.1));
        }
    }
}
