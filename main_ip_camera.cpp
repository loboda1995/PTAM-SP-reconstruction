#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui_impl_glfw_gl3.h>
#include <igl/readOFF.h>
#include <igl/opengl/glfw/Viewer.h>

#include "helpers.h"
#include "reconstruction/RealtimeReconstructionBuilder.h"
#include "plugins/IPCameraPlugin.h"
#include "plugins/ReconstructionPlugin.h"
#include "plugins/EditMeshPlugin.h"
#include "plugins/LocalizationPlugin.h"

int main(int argc, char *argv[]) {

    // Initialization
    std::string project_path =
            "/home/kristian/Documents/reconstruction_code/realtime_reconstruction_theia/dataset/ip_camera/";
    std::string images_path = project_path + "images/";
    std::string reconstruction_path = project_path + "reconstruction/";

    std::string calibration_file = project_path + "prior_calibration.txt";
    theia::CameraIntrinsicsPrior intrinsics_prior = ReadCalibration(calibration_file);

    // Initialize the viewer
    igl::opengl::glfw::Viewer viewer;
    viewer.core.is_animating = true;
    viewer.core.set_rotation_type(igl::opengl::ViewerCore::RotationType::ROTATION_TYPE_TRACKBALL);
    viewer.data().point_size = 3;

    // Setup viewer callbacks for ImGui
    viewer.callback_init = [] (igl::opengl::glfw::Viewer& v) -> bool {
        // Setup ImGui
        ImGui::CreateContext();
        ImGui_ImplGlfwGL3_Init(v.window, false);
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 5.0f;
        return false;
    };

    viewer.callback_pre_draw = [] (igl::opengl::glfw::Viewer& v) -> bool {
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();
        return false;
    };

    viewer.callback_post_draw = [] (igl::opengl::glfw::Viewer& v) -> bool {
        ImGui::Render();
        return false;
    };

    viewer.callback_shutdown = [] (igl::opengl::glfw::Viewer& v) -> bool {
        // ImGui cleanup
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    };

    // Attach camera plugin
    int image_width = intrinsics_prior.image_width;
    int image_height = intrinsics_prior.image_height;
    IPCameraPlugin camera_plugin(image_width, image_height, images_path);
    viewer.plugins.push_back(&camera_plugin);

    // Attach reconstruction plugin
    ReconstructionPlugin::Parameters parameters;
    theia::RealtimeReconstructionBuilder::Options options = SetRealtimeReconstructionBuilderOptions();
    std::shared_ptr<std::vector<std::string>> image_names = camera_plugin.get_captured_image_names();

    ReconstructionPlugin reconstruction_plugin(parameters,
                                               images_path,
                                               reconstruction_path,
                                               image_names,
                                               options,
                                               intrinsics_prior);
    viewer.plugins.push_back(&reconstruction_plugin);

    // Attach localization plugin
    // LocalizationPlugin localization_plugin(images_path,
    //                                        reconstruction_plugin.get_reconstruction_builder(),
    //                                        camera_plugin.get_current_frame());
    // viewer.plugins.push_back(&localization_plugin);

    // Attach edit mesh plugin
    EditMeshPlugin::Parameters edit_mesh_parameters;
    EditMeshPlugin edit_mesh_plugin(edit_mesh_parameters,
                                    reconstruction_path);
    viewer.plugins.push_back(&edit_mesh_plugin);

    // Start viewer
    viewer.launch();
}