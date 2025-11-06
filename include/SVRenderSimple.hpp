#ifndef SV_RENDER_SIMPLE_HPP
#define SV_RENDER_SIMPLE_HPP

#include "SVConfig.hpp"
#include "Bowl.hpp"
#include "OGLShader.hpp"
#include "Model.hpp"
#include <opencv2/core/cuda.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

/**
 * @brief Simple fixed-view camera (no mouse controls)
 */
struct Camera {
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float zoom;
    
    Camera() : position(0.0f, CAMERA_POSITION_Y, CAMERA_POSITION_Z),
               front(0.0f, 0.0f, -1.0f),
               up(0.0f, 1.0f, 0.0f),
               zoom(CAMERA_FOV) {}
    
    glm::mat4 getView() const {
        return glm::lookAt(position, position + front, up);
    }
    
    float getCamZoom() const { return zoom; }
};

/**
 * @brief Simplified OpenGL Renderer (No mouse controls)
 * 
 * Renders the stitched surround view texture on a spherical bowl
 * with optional 3D car model overlay.
 * 
 * Features:
 * - Spherical/paraboloid bowl geometry
 * - Texture mapping from stitched output
 * - 3D car model rendering
 * - Fixed camera position (no user controls)
 */
class SVRenderSimple {
public:
    /**
     * @brief Constructor
     * @param width Window width
     * @param height Window height
     */
    SVRenderSimple(int width, int height);
    ~SVRenderSimple();
    
    /**
     * @brief Initialize renderer
     * @param car_model_path Path to 3D car model (.obj)
     * @param bowl_vert_shader Bowl vertex shader path
     * @param bowl_frag_shader Bowl fragment shader path
     * @param car_vert_shader Car vertex shader path
     * @param car_frag_shader Car fragment shader path
     * @return true if initialization successful
     */
    bool init(const std::string& car_model_path,
              const std::string& bowl_vert_shader,
              const std::string& bowl_frag_shader,
              const std::string& car_vert_shader,
              const std::string& car_frag_shader);
    
    /**
     * @brief Render a frame
     * @param stitched_texture Stitched surround view texture (GPU)
     * @return true if successful
     */
    bool render(const cv::cuda::GpuMat& stitched_texture);
    
    /**
     * @brief Check if window should close
     * @return true if user pressed ESC or closed window
     */
    bool shouldClose() const;
    
private:
    /**
     * @brief Setup bowl geometry
     */
    void setupBowl();
    
    /**
     * @brief Setup car model
     * @param model_path Path to model file
     * @param vert_shader Vertex shader path
     * @param frag_shader Fragment shader path
     */
    void setupCarModel(const std::string& model_path,
                       const std::string& vert_shader,
                       const std::string& frag_shader);
    
    /**
     * @brief Upload texture from GPU to OpenGL
     * @param frame GPU frame data
     */
    void textureUpload(const cv::cuda::GpuMat& frame);
    
    // Bowl geometry data (ADD THESE)
    std::vector<float> bowl_vertices;
    std::vector<unsigned int> bowl_indices;
    size_t bowl_index_count;
    // Window
    GLFWwindow* window;
    int screen_width;
    int screen_height;
    float aspect_ratio;
    
    // Camera (fixed position, no controls)
    Camera camera;
    
    // Bowl rendering
    ConfigBowl bowl_config;
    Bowl bowl_geometry;
    OGLShader bowl_shader;
    unsigned int bowl_VAO;
    unsigned int bowl_VBO;
    unsigned int bowl_EBO;
    
    // Car model rendering
    std::unique_ptr<Model> car_model;
    std::unique_ptr<OGLShader> car_shader;
    glm::mat4 car_transform;
    
    // Texture handling
    unsigned int texture_id;
    unsigned int pbo_id;  // Pixel Buffer Object for fast upload
    
    bool is_init;
};

#endif // SV_RENDER_SIMPLE_HPP
