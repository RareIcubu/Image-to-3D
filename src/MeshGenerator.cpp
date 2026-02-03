#include "MeshGenerator.h"
#include <QDebug>
#include <QFileInfo>

// Open3D Headers - POPRAWIONE (małe litery 'open3d')
#include <open3d/geometry/PointCloud.h>
#include <open3d/geometry/TriangleMesh.h>
#include <open3d/geometry/KDTreeFlann.h>
#include <open3d/io/PointCloudIO.h>
#include <open3d/io/TriangleMeshIO.h>

bool MeshGenerator::plyToMesh(const QString &inputPly, const QString &outputObj, const MeshParams &params) {
    qDebug() << "[MeshGenerator] Loading Cloud:" << inputPly;

    // 1. Wczytanie chmury punktów
    auto pcd = std::make_shared<open3d::geometry::PointCloud>();
    
    // Używamy open3d::io::ReadPointCloud
    if (!open3d::io::ReadPointCloud(inputPly.toStdString(), *pcd)) {
        qWarning() << "[MeshGenerator] Failed to read PLY.";
        return false;
    }

    if (pcd->points_.empty()) {
        qWarning() << "[MeshGenerator] Point cloud is empty.";
        return false;
    }

    // 2. Estymacja Normalnych
    if (!pcd->HasNormals()) {
        qDebug() << "[MeshGenerator] Estimating Normals...";
        pcd->EstimateNormals(open3d::geometry::KDTreeSearchParamHybrid(20.0, 30));
        pcd->OrientNormalsConsistentTangentPlane(100);
    }

    qDebug() << "[MeshGenerator] Running Poisson Reconstruction (Depth:" << params.depth << ")...";

    // 3. Rekonstrukcja Poissona
    auto mesh_tuple = open3d::geometry::TriangleMesh::CreateFromPointCloudPoisson(
        *pcd, params.depth, 0, 1.1f 
    );

    auto mesh = std::get<0>(mesh_tuple);
    auto densities = std::get<1>(mesh_tuple);

    // 4. Pruning (Przycinanie)
    if (params.densityThreshold > 0.0f && !densities.empty()) {
        qDebug() << "[MeshGenerator] Pruning low density vertices (Threshold:" << params.densityThreshold << ")...";
        
        std::vector<double> sorted_densities = densities;
        std::sort(sorted_densities.begin(), sorted_densities.end());
        
        size_t threshold_idx = static_cast<size_t>(sorted_densities.size() * params.densityThreshold);
        if (threshold_idx >= sorted_densities.size()) threshold_idx = sorted_densities.size() - 1;
        double threshold_val = sorted_densities[threshold_idx];

        std::vector<size_t> indices_to_remove;
        for (size_t i = 0; i < densities.size(); ++i) {
            if (densities[i] < threshold_val) {
                indices_to_remove.push_back(i);
            }
        }
        
        mesh->RemoveVerticesByIndex(indices_to_remove);
        mesh->RemoveDegenerateTriangles();
        mesh->RemoveUnreferencedVertices();
    }

    // 5. Malowanie Kolorów (Reprojection)
    if (pcd->HasColors()) {
        qDebug() << "[MeshGenerator] Painting mesh vertices from point cloud...";
        open3d::geometry::KDTreeFlann kdtree(*pcd);
        std::vector<Eigen::Vector3d> new_colors;
        new_colors.reserve(mesh->vertices_.size());

        for (const auto& v : mesh->vertices_) {
            std::vector<int> indices(1);
            std::vector<double> dists(1);
            kdtree.SearchKNN(v, 1, indices, dists);
            new_colors.push_back(pcd->colors_[indices[0]]);
        }
        mesh->vertex_colors_ = new_colors;
    }

    // 6. Post-Processing
    if (params.smoothIterations > 0) {
        qDebug() << "[MeshGenerator] Smoothing mesh...";
        mesh = mesh->FilterSmoothLaplacian(params.smoothIterations, 0.5);
    }

    // 7. Zapis do pliku (WYMUSZENIE .OBJ)
    QString finalOutput = outputObj;
    
    // Zawsze zmieniaj na .obj
    if (finalOutput.endsWith(".ply", Qt::CaseInsensitive)) {
        finalOutput.replace(".ply", ".obj", Qt::CaseInsensitive);
    }

    qDebug() << "[MeshGenerator] Saving Mesh as OBJ to:" << finalOutput;
    
    mesh->ComputeVertexNormals(); 
    
    // write_ascii = true
    bool success = open3d::io::WriteTriangleMesh(
        finalOutput.toStdString(), 
        *mesh, 
        true, // write_ascii
        false // compressed
    );

    if (success) {
        qDebug() << "[MeshGenerator] Mesh saved successfully.";
    } else {
        qCritical() << "[MeshGenerator] Failed to save mesh!";
    }

    return success;
}
