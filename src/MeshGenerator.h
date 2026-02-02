#ifndef MESHGENERATOR_H
#define MESHGENERATOR_H

#include <QString>
#include <open3d/Open3D.h>

class MeshGenerator {
public:
    struct MeshParams {
        int depth;
        float densityThreshold;
        int smoothIterations;

        // Jawny konstruktor
        MeshParams(int d = 9, float dt = 0.1f, int si = 0)
            : depth(d), densityThreshold(dt), smoothIterations(si) {}
    };

    // Główna funkcja (bez wartości domyślnych, aby uniknąć problemów w .cpp)
    static bool plyToMesh(const QString &inputPly, const QString &outputObj, const MeshParams &params);
    
    // Przeciążenie dla wygody (domyślne parametry)
    // To jest implementowane inline w nagłówku, więc kompilator zawsze to widzi.
    static bool plyToMesh(const QString &inputPly, const QString &outputObj) {
        return plyToMesh(inputPly, outputObj, MeshParams{});
    }

private:
    static std::shared_ptr<open3d::geometry::TriangleMesh> postProcessMesh(
        std::shared_ptr<open3d::geometry::TriangleMesh> mesh, 
        const MeshParams &params
    );
};

#endif // MESHGENERATOR_H
