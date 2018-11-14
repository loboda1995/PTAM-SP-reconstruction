#ifndef SANDBOX_NBV_NEXTBESTVIEW_H
#define SANDBOX_NBV_NEXTBESTVIEW_H

#include <string>
#include <memory>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <OpenMVS/MVS.h>
#include <glm/glm.hpp>
#include <optim.hpp>

#include "SourceShader.h"
#include "MeasureMesh.h"
#include "FaceIdMesh.h"

class NextBestView {
public:
    explicit NextBestView(std::shared_ptr<MVS::Scene> mvs_scene);
    void Initialize();

    std::vector<unsigned int>
    RenderFaceIdFromCamera(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);

    std::vector<float>
    RenderMeasureFromCamera(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);

    std::unordered_set<unsigned int>
    VisibleFaces(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);

    std::unordered_map<unsigned int, double>
    FaceAngles(const std::unordered_set<unsigned int>& faces, const glm::mat4& view_matrix);

    std::unordered_map<unsigned int, double>
    FaceDistances(const std::unordered_set<unsigned int>& faces, const glm::mat4& view_matrix);

    // std::unordered_map<int, double> CameraAngles(const glm::mat4& view_matrix);
    std::unordered_map<int, double> CameraDistances(const glm::mat4& view_matrix);
    int ClosestCameraID(const glm::mat4& view_matrix);

    std::vector<double> PixelsPerArea();
    std::vector<double> FaceArea();

    double CostFunction(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);
    double CostFunctionPosition(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);
    double CostFunctionRotation(const glm::mat4& view_matrix, int image_width, int image_height, double focal_y);

private:
    void UpdateFaceIdMesh();
    void UpdateMeasureMesh(const std::vector<double>& measure);

public:
    // Reconstruction members
    std::shared_ptr<MVS::Scene> mvs_scene_;

private:
    // Rendering members
    std::unique_ptr<SourceShader> faceid_shader_;
    std::unique_ptr<FaceIdMesh> faceid_mesh_;

    std::unique_ptr<SourceShader> measure_shader_;
    std::unique_ptr<MeasureMesh> measure_mesh_;

    // Parameters
    double downscale_factor_ = 4.0;
    double visibility_ratio_target_ = 0.8;
    int visible_faces_target_ = 20;

    // Speedup variables
    std::vector<double> ppa_;
    std::vector<glm::vec3> face_centers_;
    std::vector<glm::vec3> face_normals_;
    std::vector<std::unordered_set<unsigned int>> visible_faces_;

    // Shaders
    const std::string faceid_vert_source =
            "#version 400 core\n"
            "layout (location = 0) in vec3 in_position;\n"
            "layout (location = 1) in uint in_face_id;\n"
            "flat out uint face_id;\n"
            "uniform mat4 model;\n"
            "uniform mat4 view;\n"
            "uniform mat4 projection;\n"
            "void main()\n"
            "{\n"
            "    face_id = in_face_id;\n"
            "    gl_Position = projection * view * model * vec4(in_position, 1.0);\n"
            "}\n";

    const std::string faceid_frag_source =
            "#version 400 core\n"
            "out uint out_face_id;\n"
            "flat in uint face_id;\n"
            "void main()\n"
            "{\n"
            "    out_face_id = face_id;\n"
            "}\n";

    const std::string measure_vert_source =
            "#version 400 core\n"
            "layout (location = 0) in vec3 in_position;\n"
            "layout (location = 1) in float in_measure;\n"
            "flat out float measure;\n"
            "uniform mat4 model;\n"
            "uniform mat4 view;\n"
            "uniform mat4 projection;\n"
            "void main()\n"
            "{\n"
            "    measure = in_measure;\n"
            "    gl_Position = projection * view * model * vec4(in_position, 1.0);\n"
            "}\n";

    const std::string measure_frag_source =
            "#version 400 core\n"
            "out float out_measure;\n"
            "flat in float measure;\n"
            "void main()\n"
            "{\n"
            "    out_measure = measure;\n"
            "}\n";
};

#endif //SANDBOX_NBV_NEXTBESTVIEW_H