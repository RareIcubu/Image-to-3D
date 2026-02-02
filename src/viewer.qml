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

    // =========================================================
    // LICZNIK FPS (DebugView) - PRZYWRÓCONY
    // =========================================================
    DebugView {
        source: view
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 5
        opacity: 0.8
        z: 2000 // Zawsze na wierzchu
    }

    // =========================================================
    // LEWY PANEL: INFO & KAMERA
    // =========================================================
    Rectangle {
        id: infoPanel
        z: 1000
        color: "#CC101010"
        width: 320
        height: 320
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 10
        radius: 8
        border.color: view.activeFocus ? "#00FF00" : "#444"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 8
            
            Text { text: "INFO & STEROWANIE"; color: "cyan"; font.bold: true; font.pixelSize: 14 }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }

            Text { 
                text: "Typ: " + (root.isMesh ? "MESH (Siatka)" : (root.isCloud ? "CHMURA PUNKTÓW" : "-"))
                color: root.isMesh ? "#88FF88" : "#FFFF88"
                font.pixelSize: 12
                font.bold: true
            }
            
            Text { 
                text: "Rozmiar źródłowy: " + debugSizeString
                color: "#AAAAAA" 
                font.pixelSize: 11 
            }
            Text { 
                text: "Auto-Skala: x" + transformNode.autoScaleFactor.toFixed(2)
                color: "orange" 
                font.pixelSize: 12
                font.bold: true
            }
            
            // Przycisk Auto-Fit
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 35
                background: Rectangle { 
                    color: parent.down ? "#005A9C" : "#007ACC"
                    radius: 4 
                }
                contentItem: Text { 
                    text: "CENTRUUJ I NORMALIZUJ"
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true 
                }
                onClicked: { 
                    if (root.isMesh && loader.bounds) calculateAutoFit(loader.bounds);
                    else if (root.isCloud && plyModel.bounds) calculateAutoFit(plyModel.bounds);
                    view.forceActiveFocus(); 
                }
            }
            
            // Checkbox 2D/AI
            CheckBox {
                checked: root.fixBackFace
                onCheckedChanged: {
                    root.fixBackFace = checked;
                    view.forceActiveFocus();
                }
                indicator: Rectangle {
                    implicitWidth: 18; implicitHeight: 18
                    radius: 3
                    color: parent.checked ? "#44FF44" : "#444"
                    border.color: "#666"
                }
                contentItem: Text { 
                    text: "Tryb 2D/AI (Odwróć oś Z)"
                    color: "lightgray"
                    font.pixelSize: 12
                    leftPadding: 10
                    verticalAlignment: Text.AlignVCenter 
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
            Text { text: "Mysz: LPM=Obrót | PPM=Przesuwanie | Scroll=Zoom"; color: "gray"; font.pixelSize: 10 }
            Text { text: "Klawisze: WASD=Latanie | Shift=Szybko"; color: "gray"; font.pixelSize: 10 }
        }
    }

    // =========================================================
    // PRAWY PANEL: EDYCJA
    // =========================================================
    Rectangle {
        id: transformPanel
        z: 1000
        color: "#CC101010"
        width: 320 
        height: parent.height - 40
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 20
        radius: 8
        border.color: "#00AAFF"
        border.width: 1

        ScrollView {
            anchors.fill: parent
            anchors.margins: 10
            clip: true

            ColumnLayout {
                width: parent.width - 20
                spacing: 12

                Text { 
                    text: "EDYCJA MODELU"; color: "#00AAFF"; font.bold: true; font.pixelSize: 14; 
                    Layout.alignment: Qt.AlignHCenter 
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }

                // --- SKALA ---
                Text { text: "SKALA CAŁKOWITA"; color: "white"; font.pixelSize: 11; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Slider {
                        id: scaleSlider
                        Layout.fillWidth: true
                        from: 0.01; to: 5.0; value: 1.0
                    }
                    Text { 
                        text: scaleSlider.value.toFixed(2) + "x"
                        color: "white"
                        font.pixelSize: 11
                        Layout.preferredWidth: 40
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#333" }

                // --- POZYCJA ---
                Text { text: "PRZESUWANIE (Position)"; color: "orange"; font.pixelSize: 11; font.bold: true }
                
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "X"; color: "#FF4444"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: posX; Layout.fillWidth: true; from: -500; to: 500; value: 0 }
                    Text { text: posX.value.toFixed(0); color: "#FFAAAA"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Y"; color: "#44FF44"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: posY; Layout.fillWidth: true; from: -500; to: 500; value: 0 }
                    Text { text: posY.value.toFixed(0); color: "#AAFFAA"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Z"; color: "#4444FF"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: posZ; Layout.fillWidth: true; from: -500; to: 500; value: 0 }
                    Text { text: posZ.value.toFixed(0); color: "#AAAAFF"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#333" }

                // --- ROTACJA ---
                Text { text: "OBRACANIE (Rotation)"; color: "lightgreen"; font.pixelSize: 11; font.bold: true }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "X"; color: "#FF4444"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: rotX; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                    Text { text: rotX.value.toFixed(0) + "°"; color: "#FFAAAA"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Y"; color: "#44FF44"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: rotY; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                    Text { text: rotY.value.toFixed(0) + "°"; color: "#AAFFAA"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Z"; color: "#4444FF"; font.bold: true; Layout.preferredWidth: 15 }
                    Slider { id: rotZ; Layout.fillWidth: true; from: 0; to: 360; value: 0 }
                    Text { text: rotZ.value.toFixed(0) + "°"; color: "#AAAAFF"; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }

                Button {
                    text: "RESETUJ WIDOK"
                    Layout.fillWidth: true
                    Layout.topMargin: 20
                    background: Rectangle { color: "#882222"; radius: 4 }
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

    // =========================================================
    // SCENA 3D
    // =========================================================
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
                        fitTimer.start();
                    }
                }
            }

            // CHMURA
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
        
        if (bMin.length() === 0 && bMax.length() === 0) return;

        var sizeVec = bMax.minus(bMin);
        var maxDim = Math.max(sizeVec.x, Math.max(sizeVec.y, sizeVec.z));
        
        debugSizeString = maxDim.toFixed(4);
        console.log("QML AutoFit: Size=" + maxDim);

        if (maxDim <= 0.000001) return;

        var targetSize = 300.0; 
        var factor = targetSize / maxDim;
        
        transformNode.autoScaleFactor = factor;

        var center = bMin.plus(bMax).times(0.5);
        if (root.isMesh) loader.position = center.times(-1);
        else plyModel.position = center.times(-1);

        blockUpdates = true;
        scaleSlider.value = 1.0;
        posX.value = 0; posY.value = 0; posZ.value = 0;
        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
        blockUpdates = false;
        
        camera.position = Qt.vector3d(0, 0, 500); 
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
