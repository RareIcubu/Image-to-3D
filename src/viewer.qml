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

    // --- LOGIKA NAPRAWY MODELI ---
    property bool fixBackFace: false 
    property string currentSource: ""
    property bool isPly: currentSource.toString().toLowerCase().endsWith(".ply")

    // --- LEWY HUD: INFO ---
    Rectangle {
        id: infoPanel
        z: 1000
        color: "#AA000000"
        width: 320
        height: 280 // Zwiększyłem wysokość, żeby zmieścić CheckBox
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
            
            // --- NOWOŚĆ: Przełącznik dla modeli AI ---
            CheckBox {
                id: aiModeCheck
                text: "Tryb 2D/AI (Napraw tył)"
                checked: root.fixBackFace
                onCheckedChanged: {
                    root.fixBackFace = checked;
                    view.forceActiveFocus();
                }
                contentItem: Text {
                    text: parent.text
                    color: "lightgreen" // Wyróżniający się kolor
                    font.bold: true
                    leftPadding: parent.indicator.width + parent.spacing
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Text { text: "WASD = Latanie | Shift = Szybko"; color: "gray"; font.pixelSize: 10 }
            Text { text: "LPM + Mysz = Rozglądanie"; color: "gray"; font.pixelSize: 10 }
        }
    }

    property string modelDimsString: "-"

    // --- PRAWY HUD: TRANSFORMACJE (Bez zmian, tylko formatowanie) ---
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

                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
                Text { text: "ZOOM OBIEKTU: " + scaleSlider.value.toFixed(2) + "x"; color: "white"; font.bold: true }
                Slider {
                    id: scaleSlider
                    Layout.fillWidth: true
                    from: 0.1; to: 10.0; value: 1.0 
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: "#555" }
                Text { text: "PRZESUWANIE"; color: "orange" }
                Text { text: "X: " + posX.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posX; Layout.fillWidth: true; from: -10000; to: 10000; value: 0 }
                Text { text: "Y: " + posY.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posY; Layout.fillWidth: true; from: -10000; to: 10000; value: 0 }
                Text { text: "Z: " + posZ.value.toFixed(0); color: "gray"; font.pixelSize: 10 }
                Slider { id: posZ; Layout.fillWidth: true; from: -10000; to: 10000; value: 0 }

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
            depthPrePassEnabled: false 
        }

        PerspectiveCamera { id: camera; z: 200; y: 100; clipNear: 1.0; clipFar: 100000.0 }
        
        DirectionalLight { eulerRotation.x: -45; eulerRotation.y: -45; brightness: 1.5; castsShadow: false }
        DirectionalLight { eulerRotation.x: 45; eulerRotation.y: 45; brightness: 1.2; color: "#FFDEAD" }
        PointLight { position: camera.position; brightness: 2.0; color: "white" }

        AxisHelper { enableXZGrid: true; gridColor: "#555"} 

        // --- KONTENER TRANSFORMACJI ---
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

            // 1. Loader dla formatów standardowych (GLB, OBJ, STL)
            RuntimeLoader {
                id: loader
                source: !root.isPly ? root.currentSource : ""
                instancing: null
                visible: !root.isPly
                
                onStatusChanged: {
                    if (status === RuntimeLoader.Ready && !root.isPly) {
                        if (!blockUpdates) calculateAutoFit(loader.bounds);
                    }
                }
            }

            // 2. Loader dla PLY (Native Points -> Fake Triangles)
            Model {
                id: plyModel
                visible: root.isPly
                geometry: PointCloudGeometry {
                    id: pcGeo
                    source: root.isPly ? root.currentSource : ""
                }
                
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    cullMode: PrincipledMaterial.NoCulling
                    baseColor: "white"
                    pointSize: 4.0 // Make points visible
                }
                
                castsShadows: false
                receivesShadows: false
                pickable: false
                
                onBoundsChanged: {
                     console.log("BOUNDS CHANGED: " + bounds.minimum + " -> " + bounds.maximum);
                     if (root.isPly && !blockUpdates) calculateAutoFit(bounds);
                }
            }
        }

        WasdController {
            controlledObject: camera
            speed: 5.0; shiftSpeed: 100.0
            keysEnabled: true; mouseEnabled: true
        }
    }
    DebugView {
        source: view
    }

    property bool blockUpdates: false

    function calculateAutoFit(bounds) {
        if (!bounds) {
            console.log("QML: calculateAutoFit called with null bounds");
            return;
        }

        var bMin = bounds.minimum;
        var bMax = bounds.maximum;
        console.log("QML: calculateAutoFit. Min:" + bMin + " Max:" + bMax);

        var sizeVec = bMax.minus(bMin);
        var maxDim = Math.max(sizeVec.x, Math.max(sizeVec.y, sizeVec.z));
        
        console.log("QML: Model size: " + sizeVec + " MaxDim: " + maxDim);

        if (maxDim <= 0.000001) {
            console.log("QML: MaxDim too small, aborting fit.");
            return; 
        }

        // 1. Centrowanie
        var center = bMin.plus(bMax).times(0.5);
        // loader.position = center.times(-1); // To działa tylko dla loadera, musimy przesunąć geometrię lub node
        // Najlepiej przesunąć Node, ale Node jest nadrzędny. 
        // Zrobimy offset wewnątrz geometrii? Nie, ustawmy position RuntimeLoadera/Modelu
        if (!root.isPly) loader.position = center.times(-1);
        else plyModel.position = center.times(-1);

        // 2. Normalizacja do 1000 jednostek
        var targetSize = 1000.0;
        transformNode.autoScaleFactor = targetSize / maxDim;

        // 3. Reset UI
        blockUpdates = true;
        scaleSlider.value = 1.0;
        
        // 4. Ustawienie na podłodze
        var normalizedHeight = sizeVec.y * transformNode.autoScaleFactor;
        posY.value = normalizedHeight / 2.0;
        posX.value = 0; posZ.value = 0;
        rotX.value = 0; rotY.value = 0; rotZ.value = 0;
        blockUpdates = false;

        // 5. Kamera
        camera.position = Qt.vector3d(0, normalizedHeight, 250);
        camera.lookAt(Qt.vector3d(0, normalizedHeight/2.0, 0));
    }

    function loadModel(path) {
        console.log("QML: loadModel called with: " + path);
        blockUpdates = false;
        currentSource = path;
        console.log("QML: currentSource set. isPly: " + isPly);
        view.forceActiveFocus();
        
        // Force update if needed
        if (isPly && plyModel.bounds) {
             console.log("QML: Checking existing bounds: " + plyModel.bounds.minimum);
             // Manually trigger fit if bounds are already valid and non-zero size
             // Note: bounds might be default if not yet loaded, but we check just in case
             if (plyModel.bounds.maximum.x !== plyModel.bounds.minimum.x) {
                  console.log("QML: Triggering manual fit (bounds exist)");
                  calculateAutoFit(plyModel.bounds);
             }
        }
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
