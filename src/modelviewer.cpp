#include "ModelViewer.h"
#include <QVBoxLayout>
#include <Qt3DRender/QPointLight>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QCamera>
// Essential includes for the 3D Window and Renderer
#include <Qt3DExtras/QForwardRenderer>
#include <QtGui/QColor>

ModelViewer::ModelViewer(QWidget *parent) : QWidget(parent) {
    // 1. Create the 3D Window
    m_view = new Qt3DExtras::Qt3DWindow();

    // Explicitly check for valid frame graph before setting color
    if (m_view->defaultFrameGraph()) {
        m_view->defaultFrameGraph()->setClearColor(QColor(40, 40, 40)); // Dark Gray Background
    }

    // 2. Wrap it in a container widget
    m_container = QWidget::createWindowContainer(m_view);

    // 3. Set up layout for this widget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_container);
    layout->setContentsMargins(0, 0, 0, 0);

    // 4. Scene Root
    m_rootEntity = new Qt3DCore::QEntity();

    // 5. Camera Setup
    Qt3DRender::QCamera *camera = m_view->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 0, 20.0f));
    camera->setViewCenter(QVector3D(0, 0, 0));

    // 6. Camera Controller (Orbit)
    m_camController = new Qt3DExtras::QOrbitCameraController(m_rootEntity);
    m_camController->setCamera(camera);
    m_camController->setLinearSpeed(50.0f);
    m_camController->setLookSpeed(180.0f);

    // 7. Light (Attached to Camera so it follows view)
    Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    Qt3DRender::QPointLight *light = new Qt3DRender::QPointLight(lightEntity);
    light->setColor("white");
    light->setIntensity(1.0f);
    lightEntity->addComponent(light);

    // Transform for light (optional, or just attach to camera entity if needed)
    // For simplicity, we just keep a static light or you can update its position.

    // 8. Model Entity Placeholder
    m_modelEntity = new Qt3DCore::QEntity(m_rootEntity);

    m_mesh = new Qt3DRender::QMesh();
    m_material = new Qt3DExtras::QPhongMaterial();
    m_transform = new Qt3DCore::QTransform();

    // Default Material Look
    m_material->setAmbient(QColor(100, 100, 100));
    m_material->setDiffuse(QColor(200, 200, 200));

    m_modelEntity->addComponent(m_mesh);
    m_modelEntity->addComponent(m_material);
    m_modelEntity->addComponent(m_transform);

    // Set Root
    m_view->setRootEntity(m_rootEntity);
}

ModelViewer::~ModelViewer() {
    // Qt3D memory management is handled by QEntity parenting
    // m_view needs explicit delete if it wasn't parented to QWidget,
    // but createWindowContainer handles ownership usually.
}

void ModelViewer::loadModel(const QString &filePath) {
    if (filePath.isEmpty()) return;

    QUrl url = QUrl::fromLocalFile(filePath);
    m_mesh->setSource(url);

    // Reset Transform
    m_transform->setScale(1.0f);
    m_transform->setTranslation(QVector3D(0, 0, 0));
}
