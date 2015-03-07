#include "IGLOuterHullEngine.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#include <Math/MatrixUtils.h>

#include <igl/cgal/remesh_self_intersections.h>
#include <igl/outer_hull.h>
#include <igl/remove_unreferenced.h>

#include <SelfIntersectionResolver.h>

void IGLOuterHullEngine::run() {
    assert(m_vertices.cols() == 3);
    assert(m_faces.cols() == 3);

    extract_face_normals();
    check_normal_reliability();
    resolve_self_intersections();
    extract_outer_hull();
}

void IGLOuterHullEngine::extract_face_normals() {
    const size_t num_faces = m_faces.rows();
    m_normals = Matrix3Fr::Zero(num_faces, 3);
    for (size_t i=0; i<num_faces; i++) {
        const auto& f = m_faces.row(i);
        Vector3F v0 = m_vertices.row(f[0]);
        Vector3F v1 = m_vertices.row(f[1]);
        Vector3F v2 = m_vertices.row(f[2]);
        m_normals.row(i) = ((v1-v0).cross(v2-v0)).normalized();
    }
}

void IGLOuterHullEngine::check_normal_reliability() {
    if (!m_normals.allFinite()) {
        std::stringstream err_msg;
        err_msg << "Normal computation failed: found nan or inf!" << std::endl;
        err_msg << "The most likely cause is degenerated triangles!";
        throw RuntimeError(err_msg.str());
    }
}

void IGLOuterHullEngine::resolve_self_intersections() {
    const size_t num_faces = m_faces.rows();

    auto resolver = SelfIntersectionResolver::create("igl");
    resolver->set_mesh(m_vertices, m_faces);
    resolver->run();
    m_vertices = resolver->get_vertices();
    m_faces = resolver->get_faces();
    auto face_sources = resolver->get_face_sources();
    assert(face_sources.size() == m_faces.rows());
    assert((face_sources.array() < num_faces).all());

    const size_t num_out_faces = m_faces.rows();
    Matrix3Fr out_normals = Matrix3Fr::Zero(num_out_faces, 3);
    for (size_t i=0; i<num_out_faces; i++) {
        out_normals.row(i) = m_normals.row(face_sources[i]);
    }
    m_normals = out_normals;
}

void IGLOuterHullEngine::extract_outer_hull() {
    assert(m_faces.rows() == m_normals.rows());
    MatrixIr out_faces;
    VectorI ori_face_indices;
    VectorI ori_face_is_flipped;
    igl::outer_hull(
            m_vertices,
            m_faces,
            m_normals,
            out_faces,
            ori_face_indices,
            ori_face_is_flipped);

    const size_t num_faces = m_faces.rows();
    const size_t num_out_faces = out_faces.rows();
    assert(ori_face_indices.size() == num_out_faces);
    std::vector<bool> in_outer(num_faces, false);
    for (size_t i=0; i<num_out_faces; i++) {
        in_outer[ori_face_indices[i]] = true;
    }

    m_debug = VectorF::Zero(num_out_faces);
    for (size_t i=0; i<num_out_faces; i++) {
        if (ori_face_is_flipped[ori_face_indices[i]]) {
            m_debug[i] = 1;
        }
    }
    //for (size_t i=0; i<num_faces; i++) {
    //    if (ori_face_is_flipped[i]) {
    //        in_outer[i] = false;
    //    }
    //}

    std::vector<VectorI> interior_faces;
    std::vector<VectorI> exterior_faces;
    for (size_t i=0; i<num_faces; i++) {
        if (in_outer[i]) {
            exterior_faces.push_back(m_faces.row(i));
        } else {
            interior_faces.push_back(m_faces.row(i));
        }
    }

    assert(exterior_faces.size() > 0);
    m_faces = MatrixUtils::rowstack(exterior_faces);

    if (interior_faces.size() > 0) {
        m_interior_faces = MatrixUtils::rowstack(interior_faces);
    }
    m_interior_vertices = m_vertices;
}
