#include "CameraPlugin.h"
#include "webcam.h"

#include <sstream>
#include <iomanip>
#include <utility>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw_gl3.h>
#include <igl/png/readPNG.h>
#include <igl/png/writePNG.h>
#include <Eigen/Core>

CameraPlugin::CameraPlugin(std::string device, int width, int height, std::string output_path)
        : device_string_(std::move(device)), image_width_(width), image_height_(height), output_path_(std::move(output_path)) {

    red_ = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Zero(image_width_, image_height_);
    green_ = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Zero(image_width_, image_height_);
    blue_ = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Zero(image_width_, image_height_);
    alpha_ = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Constant(image_width_, image_height_, 255);

    webcam_ = std::make_unique<Webcam>(device_string_, image_width_, image_height_);
    camera_message_ = "";
    saved_frames_count_ = 0;
}

void CameraPlugin::init(igl::opengl::glfw::Viewer *_viewer) {
    ViewerPlugin::init(_viewer);

    // Create texture for camera view (needs glfw context)
    glGenTextures(1, &textureID_);
    glBindTexture(GL_TEXTURE_2D, textureID_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_width_, image_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool CameraPlugin::post_draw() {
    // Setup window
    float window_width = 480.0f;
    ImGui::SetNextWindowSize(ImVec2(window_width, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - window_width, 0.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_NoSavedSettings);

    // Get frame from webcam
    auto frame = webcam_->frame();

    // Replace texture with new frame
    glBindTexture(GL_TEXTURE_2D, textureID_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_width_, image_height_, GL_RGB, GL_UNSIGNED_BYTE, frame.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Add an image
    float width = ImGui::GetWindowContentRegionWidth();
    float height = width * ((float) image_height_ / image_width_);
    ImGui::Image(reinterpret_cast<GLuint*>(textureID_), ImVec2(width, height));

    // Add a button
    if (ImGui::Button("Capture frame", ImVec2(-1,0))) {
        // Store frame on disk
        std::stringstream ss;
        ss << std::setw(3) << std::setfill('0') << std::to_string(saved_frames_count_);
        std::string filename = output_path_ + "frame" + ss.str() + ".png";

        red_ = Eigen::Map<Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>, 0, Eigen::Stride<Eigen::Dynamic, 3>>
                (frame.data, image_width_, image_height_, Eigen::Stride<Eigen::Dynamic,3>(image_width_*3, 3));
        green_ = Eigen::Map<Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>, 0, Eigen::Stride<Eigen::Dynamic, 3>>
                (frame.data + 1, image_width_, image_height_, Eigen::Stride<Eigen::Dynamic,3>(image_width_*3, 3));
        blue_ = Eigen::Map<Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>, 0, Eigen::Stride<Eigen::Dynamic, 3>>
                (frame.data + 2, image_width_, image_height_, Eigen::Stride<Eigen::Dynamic,3>(image_width_*3, 3));

        igl::png::writePNG(red_.rowwise().reverse(), green_.rowwise().reverse(), blue_.rowwise().reverse(), alpha_, filename);

        camera_message_ = "Image saved to: " + filename;
        saved_frames_count_++;
    }

    // Add camera message
    ImGui::Text("%s", camera_message_.c_str());

    ImGui::End();
    return false;
}

// Mouse IO
bool CameraPlugin::mouse_down(int button, int modifier)
{
    ImGui_ImplGlfwGL3_MouseButtonCallback(viewer->window, button, GLFW_PRESS, modifier);
    return ImGui::GetIO().WantCaptureMouse;
}

bool CameraPlugin::mouse_up(int button, int modifier)
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool CameraPlugin::mouse_move(int mouse_x, int mouse_y)
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool CameraPlugin::mouse_scroll(float delta_y)
{
    ImGui_ImplGlfwGL3_ScrollCallback(viewer->window, 0.f, delta_y);
    return ImGui::GetIO().WantCaptureMouse;
}

// Keyboard IO
bool CameraPlugin::key_pressed(unsigned int key, int modifiers)
{
    ImGui_ImplGlfwGL3_CharCallback(nullptr, key);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool CameraPlugin::key_down(int key, int modifiers)
{
    ImGui_ImplGlfwGL3_KeyCallback(viewer->window, key, 0, GLFW_PRESS, modifiers);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool CameraPlugin::key_up(int key, int modifiers)
{
    ImGui_ImplGlfwGL3_KeyCallback(viewer->window, key, 0, GLFW_RELEASE, modifiers);
    return ImGui::GetIO().WantCaptureKeyboard;
}