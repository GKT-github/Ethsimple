#ifndef SV_APP_SIMPLE_HPP
#define SV_APP_SIMPLE_HPP

#include "SVConfig.hpp"
#include "SVEthernetCamera.hpp"
#include "SVStitcherSimple.hpp"
#include "SVRenderSimple.hpp"
#include <memory>
#include <array>
#include <string>

/**
 * @brief Simplified Surround View Application
 * 
 * Main application class that orchestrates:
 * - Camera capture from 4 H.264 Ethernet streams
 * - Spherical warping and stitching
 * - Multi-band blending
 * - OpenGL bowl rendering with car overlay
 */
class SVAppSimple {
public:
    SVAppSimple();
    ~SVAppSimple();
    
    /**
     * @brief Initialize the system
     * @param calib_folder Path to folder containing calibration YAML files
     * @return true if initialization successful, false otherwise
     */
    bool init(const std::string& calib_folder);
    
    /**
     * @brief Run main loop (blocking)
     * Captures frames, stitches, and renders until stopped
     */
    void run();
    
    /**
     * @brief Stop the system
     */
    void stop();
    
private:
    // Camera source
    std::shared_ptr<MultiCameraSource> camera_source;
    std::array<Frame, NUM_CAMERAS> frames;
    
    // Stitching
    std::shared_ptr<SVStitcherSimple> stitcher;
    cv::cuda::GpuMat stitched_output;
    
    // Rendering
    std::shared_ptr<SVRenderSimple> renderer;
    
    // State
    bool is_running;
    std::string calibration_folder;
};

#endif // SV_APP_SIMPLE_HPP
