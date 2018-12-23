#include "RenderPlugin.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw_gl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

RenderPlugin::RenderPlugin(
        std::string images_path,
        std::string reconstruction_path,
        std::shared_ptr<Render> render)
        : images_path_(std::move(images_path)),
          reconstruction_path_(std::move(reconstruction_path)),
          image_names_(std::make_shared<std::vector<std::string>>()),
          render_(std::move(render)) {}

void RenderPlugin::init(igl::opengl::glfw::Viewer* _viewer) {
    ViewerPlugin::init(_viewer);

    // Check for plugins
    for (int i = 0; i < viewer->plugins.size(); i++) {
        // Reconstruction plugin
        if (!reconstruction_plugin_) {
            reconstruction_plugin_ = dynamic_cast<ReconstructionPlugin*>(viewer->plugins[i]);
        }
        // NBV plugin
        if (!nbv_plugin_) {
            nbv_plugin_ = dynamic_cast<NextBestViewPlugin*>(viewer->plugins[i]);
        }
    }

    // Append mesh for camera
    viewer->append_mesh();
    VIEWER_DATA_CAMERA = static_cast<unsigned int>(viewer->data_list.size() - 1);

    // Append mesh for render cameras
    viewer->append_mesh();
    VIEWER_DATA_RENDER_CAMERAS = static_cast<unsigned int>(viewer->data_list.size() - 1);

    // Append mesh for render mesh
    viewer->append_mesh();
    VIEWER_DATA_RENDER_MESH = static_cast<unsigned int>(viewer->data_list.size() - 1);

    // Initial gizmo pose
    camera_gizmo_ = Eigen::Matrix4f::Identity();
}

bool RenderPlugin::pre_draw() {
    ImGuizmo::BeginFrame();
    return false;
}

bool RenderPlugin::post_draw() {
    // Setup window
    float window_width = 350.0f;
    ImGui::SetNextWindowSize(ImVec2(window_width, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(700.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Render", nullptr, ImGuiWindowFlags_NoSavedSettings);

    // Gizmo setup
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    Eigen::Affine3f scale_base_zoom(Eigen::Scaling(1.0f / viewer->core.camera_base_zoom));
    Eigen::Affine3f scale_zoom(Eigen::Scaling(1.0f / viewer->core.camera_zoom));
    Eigen::Matrix4f gizmo_view = scale_base_zoom * scale_zoom * viewer->core.view;


    // Initialization
    if (ImGui::TreeNodeEx("Initialization", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Filename", scene_name_, 128, ImGuiInputTextFlags_AutoSelectAll);
        if (ImGui::Button("Initialize scene", ImVec2(-1, 0))) {
            initialize_scene_callback();
        }
        if (ImGui::Checkbox("Show render mesh", &render_mesh_visible_)) {
            show_render_mesh(render_mesh_visible_);
        }
        if (ImGui::Checkbox("Show render cameras", &render_cameras_visible_)) {
            show_render_cameras(render_cameras_visible_);
        }
        ImGui::TreePop();
    }

    // Manual camera pose
    if (ImGui::TreeNodeEx("Manual camera pose", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show render camera", &camera_visible_);
        ImGui::Checkbox("Pose camera", &pose_camera_);
        ImGui::InputFloat3("Position", glm::value_ptr(camera_pos_));
        ImGui::InputFloat3("Angles", glm::value_ptr(camera_rot_));
        if (pose_camera_) {
            ImGuizmo::Manipulate(gizmo_view.data(),
                                 viewer->core.proj.data(),
                                 gizmo_operation_,
                                 gizmo_mode_,
                                 camera_gizmo_.data());

            ImGui::Text("Camera options");
            if (ImGui::RadioButton("Translate", gizmo_operation_ == ImGuizmo::TRANSLATE)) {
                gizmo_operation_ = ImGuizmo::TRANSLATE;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate", gizmo_operation_ == ImGuizmo::ROTATE)) {
                gizmo_operation_ = ImGuizmo::ROTATE;
            }
        }
        ImGui::TreePop();
    }
    show_camera();

    // Generated render poses
    if (ImGui::TreeNodeEx("Generated render poses", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("Pose index", &selected_pose_, 0, generated_poses_.size()-1)) {
            set_generated_pose_callback();
        }
        ImGui::TreePop();
    }

    // Render and save image
    if (ImGui::TreeNodeEx("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Render current pose", ImVec2(-1, 0))) {
            render_callback();
        }
        if (ImGui::Button("Save current render", ImVec2(-1, 0))) {
            save_render_callback();
        }
        ImGui::PushItemWidth(100.0f);
        ImGui::InputInt("Next image index", &next_image_idx_);
        ImGui::PopItemWidth();
        ImGui::TreePop();
    }

    // NBV plugin link
    if (reconstruction_plugin_ && nbv_plugin_) {
        if (ImGui::TreeNodeEx("Plugin link", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Initialize reconstruction", ImVec2(-1, 0))) {
                initialize_reconstruction_callback();
            }
            if (ImGui::Button("Compute NBV and extend", ImVec2(-1, 0))) {
                extend_reconstruction_callback();
            }
            ImGui::TreePop();
        }
    }

    // Debugging
    if (ImGui::TreeNodeEx("Debug")) {
        // Other
        if (ImGui::Button("Debug [d]", ImVec2(-1, 0))) {
            std::cout << "Render: debug button pressed" << std::endl;
        }
        ImGui::TreePop();
    }

    // Set camera transformation
    if (pose_camera_) {
        // Camera gizmo -> position and rotation
        glm::vec3 scale;
        ImGuizmo::DecomposeMatrixToComponents(
                camera_gizmo_.data(),
                glm::value_ptr(camera_pos_),
                glm::value_ptr(camera_rot_),
                glm::value_ptr(scale));
    } else {
        // Position and rotation -> camera gizmo
        glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0);
        ImGuizmo::RecomposeMatrixFromComponents(
                glm::value_ptr(camera_pos_),
                glm::value_ptr(camera_rot_),
                glm::value_ptr(scale),
                camera_gizmo_.data());
    }

    ImGui::End();
    return false;
}

std::shared_ptr<std::vector<std::string>> RenderPlugin::get_rendered_image_names() {
    return image_names_;
}

void RenderPlugin::initialize_scene_callback() {
    log_stream_ << std::endl;

    std::string tmp(scene_name_);
    std::string fullpath = reconstruction_path_ + tmp + ".mvs";
    MVS::Scene mvs_scene;
    mvs_scene.Load(fullpath);

    render_->Initialize(mvs_scene);
    set_render_mesh(mvs_scene);
    show_render_mesh(true);

    generated_poses_ = render_->GenerateRenderPoses(mvs_scene);
    set_render_cameras();
    show_render_cameras(true);

    log_stream_ << "Render: Scene initialized, loaded from: \n\t"
                << fullpath << std::endl;
}

void RenderPlugin::render_callback() {
    glm::mat4 view_world = glm::mat4(1.0f);
    glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0);
    ImGuizmo::RecomposeMatrixFromComponents(
            glm::value_ptr(camera_pos_),
            glm::value_ptr(camera_rot_),
            glm::value_ptr(scale),
            glm::value_ptr(view_world));
    glm::mat4 view_matrix = glm::inverse(view_world);

    Render::CameraIntrinsic intrinsic = render_->GetCameraIntrinsic(0);
    render_data_ = render_->RenderFromCamera(view_matrix, intrinsic);
}

void RenderPlugin::save_render_callback() {

    // Prepare filename
    std::stringstream ss;
    ss << std::setw(3) << std::setfill('0') << std::to_string(next_image_idx_);
    std::string filename = "frame" + ss.str() + ".png";
    std::string fullname = images_path_ + filename;

    Render::CameraIntrinsic intrinsic = render_->GetCameraIntrinsic(0);
    render_->SaveRender(fullname, intrinsic, render_data_);

    // Compute view matrix again
    glm::mat4 view_world = glm::mat4(1.0f);
    glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0);
    ImGuizmo::RecomposeMatrixFromComponents(
            glm::value_ptr(camera_pos_),
            glm::value_ptr(camera_rot_),
            glm::value_ptr(scale),
            glm::value_ptr(view_world));
    glm::mat4 view_matrix = glm::inverse(view_world);

    // Succsessful render
    image_names_->push_back(filename);
    rendered_poses_.push_back(view_matrix);
    next_image_idx_++;
    selected_pose_++;
    log_stream_ << "Render: Image saved to: \n\t" + fullname << std::endl;
}

void RenderPlugin::set_generated_pose_callback() {
    if (selected_pose_ >= 0 && selected_pose_ < generated_poses_.size()) {
        const glm::mat4& view_matrix = generated_poses_[selected_pose_];
        set_camera_pose(view_matrix);
    }
}

void RenderPlugin::initialize_reconstruction_callback() {
    set_generated_pose_callback();
    render_callback();
    save_render_callback();
    set_generated_pose_callback();
    render_callback();
    save_render_callback();

    reconstruction_plugin_->initialize_callback();
}

void RenderPlugin::extend_reconstruction_callback() {
    nbv_plugin_->initialize_callback();
    std::vector<glm::mat4> best_views = nbv_plugin_->get_initial_best_views();

    const glm::mat4& view_matrix = best_views.front();

    set_camera_pose(view_matrix);
    // camera_pos_ = nbv_plugin_->get_camera_pos();
    // camera_rot_ = nbv_plugin_->get_camera_rot();

    render_callback();
    save_render_callback();
    //
    // reconstruction_plugin_->extend_callback();
}

void RenderPlugin::show_camera() {
    viewer->selected_data_index = VIEWER_DATA_CAMERA;
    if (camera_visible_) {

        // Compute camera transformation
        Eigen::Matrix4f tmp;
        glm::vec3 scale = glm::vec3(1.0 / 2.0, 1.0 / 2.0, 1.0 / 2.0);
        ImGuizmo::RecomposeMatrixFromComponents(
                glm::value_ptr(camera_pos_),
                glm::value_ptr(camera_rot_),
                glm::value_ptr(scale),
                tmp.data());
        Eigen::Matrix4d camera_mat_world = tmp.cast<double>();

        // Vertices
        Eigen::MatrixXd tmp_V(5, 3);
        tmp_V << 0, 0, 0,
                0.75, 0.5, -1,
                -0.75, 0.5, -1,
                -0.75, -0.5, -1,
                0.75, -0.5, -1;

        // Faces
        Eigen::MatrixXi tmp_F(6, 3);
        tmp_F << 0, 1, 2,
                0, 2, 3,
                0, 3, 4,
                0, 4, 1,
                1, 3, 2,
                1, 4, 3;

        // Apply transformation
        tmp_V = (tmp_V.rowwise().homogeneous() * camera_mat_world.transpose()).rowwise().hnormalized();

        // Set viewer data
        viewer->data().clear();
        viewer->data().set_mesh(tmp_V, tmp_F);
        viewer->data().set_face_based(true);
        Eigen::Vector3d color = Eigen::Vector3d(255, 255, 0) / 255.0;
        viewer->data().uniform_colors(color, color, color);
    } else {
        viewer->data().clear();
    }
}
void RenderPlugin::set_camera_pose(const glm::mat4& view_matrix) {
    glm::mat4 view_world = glm::inverse(view_matrix);

    glm::vec3 scale;
    ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(view_world),
            glm::value_ptr(camera_pos_),
            glm::value_ptr(camera_rot_),
            glm::value_ptr(scale));
}

void RenderPlugin::set_render_mesh(const MVS::Scene& mvs_scene) {
    viewer->selected_data_index = VIEWER_DATA_RENDER_MESH;
    viewer->data().clear();

    // Add vertices
    int num_vertices = mvs_scene.mesh.vertices.size();
    Eigen::MatrixXd V(num_vertices, 3);
    for (int i = 0; i < num_vertices; i++) {
        MVS::Mesh::Vertex vertex = mvs_scene.mesh.vertices[i];
        V(i, 0) = vertex[0];
        V(i, 1) = vertex[1];
        V(i, 2) = vertex[2];
    }

    // Add faces
    int num_faces = mvs_scene.mesh.faces.size();
    Eigen::MatrixXi F(num_faces, 3);
    for (int i = 0; i < num_faces; i++) {
        MVS::Mesh::Face face = mvs_scene.mesh.faces[i];
        F(i, 0) = face[0];
        F(i, 1) = face[1];
        F(i, 2) = face[2];
    }

    viewer->data().set_mesh(V, F);
    viewer->data().show_lines = false;
    viewer->data().set_colors(Eigen::RowVector3d(1, 1, 1));
    show_render_mesh(true);
    center_object();
}

void RenderPlugin::show_render_mesh(bool visible) {
    render_mesh_visible_ = visible;
    viewer->selected_data_index = VIEWER_DATA_RENDER_MESH;
    viewer->data().show_faces = visible;
}

void RenderPlugin::center_object() {
    viewer->selected_data_index = VIEWER_DATA_RENDER_MESH;
    Eigen::MatrixXd points;
    points = viewer->data().V;

    if (points.rows() == 0) {
        return;
    }

    Eigen::Vector3d min_point = points.colwise().minCoeff();
    Eigen::Vector3d max_point = points.colwise().maxCoeff();
    Eigen::Vector3d center = points.colwise().mean();
    viewer->core.camera_base_translation = (-center).cast<float>();
    viewer->core.camera_translation.setConstant(0);

    Eigen::Vector3d diff = (max_point - min_point).array().abs();
    viewer->core.camera_base_zoom = static_cast<float>(2.0 / diff.maxCoeff());
    viewer->core.camera_zoom = 1.0;
}

void RenderPlugin::set_render_cameras() {
    viewer->selected_data_index = VIEWER_DATA_RENDER_CAMERAS;
    viewer->data().clear();

    // Camera in default position
    int num_vertices = 5;
    Eigen::MatrixXd default_V(num_vertices, 3);
    default_V << 0, 0, 0,
            0.75, 0.5, -1,
            -0.75, 0.5, -1,
            -0.75, -0.5, -1,
            0.75, -0.5, -1;

    int num_faces = 6;
    Eigen::MatrixXi default_F(num_faces, 3);
    default_F << 0, 1, 2,
            0, 2, 3,
            0, 3, 4,
            0, 4, 1,
            1, 3, 2,
            1, 4, 3;

    // Add cameras
    int num_views = generated_poses_.size();
    Eigen::MatrixXd cameras_V(num_views * num_vertices, 3);
    Eigen::MatrixXi cameras_F(num_views * num_faces, 3);

    int i = 0;
    for (const auto& view_matrix : generated_poses_) {
        // Camera transformation
        Eigen::Affine3f tmp(Eigen::Matrix4f::Map(glm::value_ptr(view_matrix)));
        Eigen::Affine3d transformation = tmp.inverse().cast<double>();

        Eigen::Affine3d scale(Eigen::Scaling(1.0 / 2.5));
        transformation = transformation * scale;

        // Apply transformation
        Eigen::MatrixXd transformed_V = (default_V.rowwise().homogeneous() * transformation.matrix().transpose()).rowwise().hnormalized();
        Eigen::MatrixXi transformed_F = default_F.array() + i * num_vertices;

        cameras_V.middleRows(i * num_vertices, num_vertices) = transformed_V;
        cameras_F.middleRows(i * num_faces, num_faces) = transformed_F;

        i++;
    }

    // Set viewer data
    viewer->data().set_mesh(cameras_V, cameras_F);
    viewer->data().set_face_based(true);
    Eigen::Vector3d gray_color = Eigen::Vector3d(128, 128, 128) / 255.0;
    viewer->data().uniform_colors(gray_color, gray_color, gray_color);
}

void RenderPlugin::show_render_cameras(bool visible) {
    render_cameras_visible_ = visible;
    viewer->selected_data_index = VIEWER_DATA_RENDER_CAMERAS;
    viewer->data().show_faces = visible;
    viewer->data().show_lines = visible;
}

// Mouse IO
bool RenderPlugin::mouse_down(int button, int modifier) {
    ImGui_ImplGlfwGL3_MouseButtonCallback(viewer->window, button, GLFW_PRESS, modifier);
    return ImGui::GetIO().WantCaptureMouse;
}

bool RenderPlugin::mouse_up(int button, int modifier) {
    return ImGui::GetIO().WantCaptureMouse;
}

bool RenderPlugin::mouse_move(int mouse_x, int mouse_y) {
    return ImGui::GetIO().WantCaptureMouse;
}

bool RenderPlugin::mouse_scroll(float delta_y) {
    ImGui_ImplGlfwGL3_ScrollCallback(viewer->window, 0.f, delta_y);
    return ImGui::GetIO().WantCaptureMouse;
}

// Keyboard IO
bool RenderPlugin::key_pressed(unsigned int key, int modifiers) {
    ImGui_ImplGlfwGL3_CharCallback(nullptr, key);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool RenderPlugin::key_down(int key, int modifiers) {
    ImGui_ImplGlfwGL3_KeyCallback(viewer->window, key, 0, GLFW_PRESS, modifiers);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool RenderPlugin::key_up(int key, int modifiers) {
    ImGui_ImplGlfwGL3_KeyCallback(viewer->window, key, 0, GLFW_RELEASE, modifiers);
    return ImGui::GetIO().WantCaptureKeyboard;
}