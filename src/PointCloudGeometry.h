#ifndef POINTCLOUDGEOMETRY_H
#define POINTCLOUDGEOMETRY_H

#include <QQuick3DGeometry>
#include <QVector3D>
#include <QUrl>

class PointCloudGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PointCloudGeometry)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)

public:
    PointCloudGeometry();

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    
    // Helper do centrowania kamery
    Q_INVOKABLE QVector3D center() const { return m_center; }
    
    // Helper do testów
    int vertexCount() const { return m_vertexCount; }

signals:
    void sourceChanged();

private:
    void updateData();
    void readPly(const QString &path);

    QUrl m_source;
    QVector3D m_center;
    int m_vertexCount = 0;
};

#endif // POINTCLOUDGEOMETRY_H