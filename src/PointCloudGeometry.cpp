#include "PointCloudGeometry.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

PointCloudGeometry::PointCloudGeometry()
{
    updateData();
}

void PointCloudGeometry::setSource(const QUrl &source)
{
    if (m_source == source) return;
    m_source = source;
    emit sourceChanged();
    updateData();
}

void PointCloudGeometry::updateData()
{
    clear();
    m_vertexCount = 0;
    
    if (m_source.isEmpty()) return;

    QString path = m_source.toLocalFile();
    if (path.isEmpty()) return;

    readPly(path);
}

void PointCloudGeometry::readPly(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open PLY:" << path;
        return;
    }

    int vertexCount = 0;
    bool isBinary = false;
    // Simple header parsing
    while (!file.atEnd()) {
        QByteArray line = file.readLine().trimmed();
        if (line.startsWith("format binary_little_endian")) {
            isBinary = true;
        } else if (line.startsWith("element vertex")) {
            QList<QByteArray> parts = line.split(' ');
            if (!parts.isEmpty()) vertexCount = parts.last().toInt();
        } else if (line == "end_header") {
            break;
        }
    }

    qDebug() << "PLY Reader:" << path << "Vertices:" << vertexCount << "Binary:" << isBinary;

    if (vertexCount == 0) return;
    m_vertexCount = vertexCount;

    // Output format: Pos(3 floats) + Color(4 floats: R, G, B, A) = 7 floats * 4 bytes = 28 bytes
    int stride = 7 * sizeof(float);
    
    QByteArray vertexData;
    vertexData.resize(vertexCount * stride);
    char *ptr = vertexData.data();

    QVector3D min(1e9, 1e9, 1e9), max(-1e9, -1e9, -1e9);

    if (isBinary) {
        // --- BINARY PARSING ---
        QByteArray data = file.readAll();
        const char *srcPtr = data.constData();
        int srcOffset = 0;
        int srcSize = data.size();
        
        // Guess stride based on size:
        int inputStride = (vertexCount > 0) ? (srcSize / vertexCount) : 0;
        qDebug() << "Binary PLY Stride detected:" << inputStride << "Total size:" << srcSize;
        
        if (inputStride < 12) {
             qWarning() << "Invalid PLY stride (too small):" << inputStride;
             return;
        }
        
        for (int i = 0; i < vertexCount; ++i) {
            if (srcOffset + 12 > srcSize) {
                qWarning() << "Unexpected end of file at vertex" << i;
                break;
            }

            float x, y, z;
            memcpy(&x, srcPtr + srcOffset, 4);
            memcpy(&y, srcPtr + srcOffset + 4, 4);
            memcpy(&z, srcPtr + srcOffset + 8, 4);
            
            float r = 1.0f, g = 1.0f, b = 1.0f;
            
            if (inputStride >= 15) { 
                // Assume color is at the end? Or after XYZ? 
                // Standard PLY binary often has properties in order defined in header.
                // Assuming standard "x,y,z,red,green,blue" structure where rgb are uchar.
                // Offset of RGB is usually 12.
                unsigned char rc = srcPtr[srcOffset + 12];
                unsigned char gc = srcPtr[srcOffset + 13];
                unsigned char bc = srcPtr[srcOffset + 14];
                r = rc / 255.0f; g = gc / 255.0f; b = bc / 255.0f;
            }
            
            float *p = reinterpret_cast<float*>(ptr);
            p[0] = x; p[1] = y; p[2] = z;
            p[3] = r; p[4] = g; p[5] = b; p[6] = 1.0f;
            
            ptr += stride;
            srcOffset += inputStride;
            
            min.setX(qMin(min.x(), x)); min.setY(qMin(min.y(), y)); min.setZ(qMin(min.z(), z));
            max.setX(qMax(max.x(), x)); max.setY(qMax(max.y(), y)); max.setZ(qMax(max.z(), z));

            if (i < 5) {
                qDebug() << "Point" << i << ":" << x << y << z << "RGB:" << r << g << b;
            }
        }

        qDebug() << "Binary PLY Bounds:" << min << "to" << max;

    } else {
        // --- ASCII PARSING ---
        QTextStream stream(&file);
        for (int i = 0; i < vertexCount; ++i) {
            if (stream.atEnd()) break;
            float x, y, z;
            int r = 255, g = 255, b = 255;
            stream >> x >> y >> z;
            
            // Handle remaining properties (nx, ny, nz, r, g, b, etc.)
            // Logic: Read line, split, take last 3 as RGB if available
            QString rest = stream.readLine();
            QStringList parts = rest.trimmed().split(' ', Qt::SkipEmptyParts);
            
            // Try to find color. Usually at the end.
            if (parts.size() >= 3) {
                // Heuristic: check if they look like color values (0-255 integers)
                // or just take last 3.
                bool ok1, ok2, ok3;
                int c1 = parts[parts.size()-3].toInt(&ok1);
                int c2 = parts[parts.size()-2].toInt(&ok2);
                int c3 = parts[parts.size()-1].toInt(&ok3);
                
                if (ok1 && ok2 && ok3) {
                    r = c1; g = c2; b = c3;
                }
            }

            float *p = reinterpret_cast<float*>(ptr);
            p[0] = x; p[1] = y; p[2] = z;
            p[3] = r / 255.0f; p[4] = g / 255.0f; p[5] = b / 255.0f; p[6] = 1.0f;
            
            ptr += stride;
            
            min.setX(qMin(min.x(), x)); min.setY(qMin(min.y(), y)); min.setZ(qMin(min.z(), z));
            max.setX(qMax(max.x(), x)); max.setY(qMax(max.y(), y)); max.setZ(qMax(max.z(), z));
        }
    }

    setVertexData(vertexData);
    setStride(stride);
    
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::ColorSemantic, 3 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);

    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Points);
    setBounds(min, max);
    m_center = (min + max) / 2.0f;
    
    update(); // <--- CRITICAL: Notify renderer that geometry data has changed!
}