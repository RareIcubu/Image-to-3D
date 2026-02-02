#include "MeshGenerator.h"
#include <QDebug>
#include <iostream>
#include <algorithm>
#include <vector>
#include <open3d/Open3D.h>

bool MeshGenerator::plyToMesh(const QString &inputPly, const QString &outputObj, const MeshParams &params) {
    qDebug() << "[MeshGenerator] Loading Cloud:" << inputPly;

    auto pcd = std::make_shared<open3d::geometry::PointCloud>();
    if (!open3d::io::ReadPointCloud(inputPly.toStdString(), *pcd)) {
        qWarning() << "[MeshGenerator] Failed to read PLY.";
        return false;
    }

    if (pcd->points_.empty()) {
        qWarning() << "[MeshGenerator] Point cloud is empty.";
        return false;
    }

    // 2. Estimate Normals
    pcd->EstimateNormals(
        open3d::geometry::KDTreeSearchParamHybrid(20.0, 30) 
    );
    pcd->OrientNormalsConsistentTangentPlane(100);

    qDebug() << "[MeshGenerator] Running Poisson Reconstruction (Depth:" << params.depth << ")...";

    // 3. Poisson Reconstruction
    // Open3D 0.17.0+
    auto mesh_tuple = open3d::geometry::TriangleMesh::CreateFromPointCloudPoisson(
        *pcd, params.depth, 0, 1.1f 
    );

    auto mesh = std::get<0>(mesh_tuple);
    auto densities = std::get<1>(mesh_tuple);

    // 4. Pruning
    if (params.densityThreshold > 0 && !densities.empty()) {
        qDebug() << "[MeshGenerator] Pruning low density vertices...";
        std::vector<double> sorted_densities = densities;
        std::sort(sorted_densities.begin(), sorted_densities.end());
        size_t threshold_idx = static_cast<size_t>(sorted_densities.size() * params.densityThreshold);
        if (threshold_idx >= sorted_densities.size()) threshold_idx = sorted_densities.size() - 1;
        double threshold_val = sorted_densities[threshold_idx];

        std::vector<size_t> indices_to_remove;
        for (size_t i = 0; i < densities.size(); ++i) {
            if (densities[i] < threshold_val) indices_to_remove.push_back(i);
        }
        mesh->RemoveVerticesByIndex(indices_to_remove);
    }

    // --- KOLOROWANIE MESHA ---
    if (pcd->HasColors()) {
        qDebug() << "[MeshGenerator] Painting mesh vertices from point cloud...";
        
        // Budujemy KDTree dla chmury
        open3d::geometry::KDTreeFlann kdtree(*pcd);
        std::vector<Eigen::Vector3d> new_colors;
        new_colors.reserve(mesh->vertices_.size());

        for (const auto& v : mesh->vertices_) {
            std::vector<int> indices(1);
            std::vector<double> dists(1);
            // Szukamy 1 najbliższego sąsiada
            kdtree.SearchKNN(v, 1, indices, dists);
            // Pobieramy kolor z chmury punktów (PointCloud używa colors_)
            new_colors.push_back(pcd->colors_[indices[0]]);
        }
        
        // FIX: TriangleMesh używa 'vertex_colors_', a nie 'colors_'
        mesh->vertex_colors_ = new_colors;
    }
    // ------------------------------

    // 5. Post-Processing
    if (params.smoothIterations > 0) {
        mesh = mesh->FilterSmoothLaplacian(params.smoothIterations, 0.5);
    }

    // 6. Save
    // Wymuszamy PLY, bo OBJ w Open3D słabo obsługuje vertex colors
    QString finalOutput = outputObj;
    if (outputObj.endsWith(".obj", Qt::CaseInsensitive)) {
        finalOutput.replace(".obj", ".ply", Qt::CaseInsensitive);
    }

    qDebug() << "[MeshGenerator] Saving Mesh to:" << finalOutput;
    mesh->ComputeVertexNormals(); 
    
    // Zapisujemy w formacie PLY
    return open3d::io::WriteTriangleMesh(finalOutput.toStdString(), *mesh, true, false); 
}

std::shared_ptr<open3d::geometry::TriangleMesh> MeshGenerator::postProcessMesh(
        std::shared_ptr<open3d::geometry::TriangleMesh> mesh, 
        const MeshParams &params
) {
    return mesh;
}
