import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Rectangle {
    anchors.fill: parent
    color: "black"

    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#222222"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: camera
            z: 300
        }

        DirectionalLight {
            eulerRotation.x: -30
            eulerRotation.y: -30
        }

        // --- The 3D Model Loader ---
        // This allows loading OBJ/GLTF files at runtime
        Node {
            id: modelNode

            // Helpful grid to see where "floor" is
            GridGeometry {
                horizontalLines: 20
                verticalLines: 20
                step: 10
            }
        }
    }

    // Function callable from C++ to load a file
    function loadModel(path) {
        console.log("Loading model from:", path)
        // We use a dynamic loader component for runtime files
        var component = Qt.createComponent("ModelLoader.qml");
        if (component.status === Component.Ready) {
            var object = component.createObject(modelNode, {source: path});
        }
    }
}
