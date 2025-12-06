#pragma once

#include <QWidget>
#include <QUrl>

// Qt3D Includes
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DCore/QTransform>

class ModelViewer : public QWidget {
    Q_OBJECT

public:
    explicit ModelViewer(QWidget *parent = nullptr);
    ~ModelViewer();

    // function to load the file
    void loadModel(const QString &filePath);

private:
    // The actual 3D rendering window
    Qt3DExtras::Qt3DWindow *m_view;

    // The container widget that holds the 3D window
    QWidget *m_container;

    // Scene Graph Root
    Qt3DCore::QEntity *m_rootEntity;

    // The Model Entity Components
    Qt3DCore::QEntity *m_modelEntity;
    Qt3DRender::QMesh *m_mesh;
    Qt3DExtras::QPhongMaterial *m_material;
    Qt3DCore::QTransform *m_transform;

    // Camera Controller
    Qt3DExtras::QOrbitCameraController *m_camController;
};
