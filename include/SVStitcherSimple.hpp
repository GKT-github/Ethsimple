#ifndef SV_STITCHER_SIMPLE_HPP
#define SV_STITCHER_SIMPLE_HPP

#include "SVConfig.hpp"
#include "SVBlender.hpp"
#include "SVGainCompensator.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <vector>
#include <string>
#include <memory>

/**
 * @brief Simplified Stitcher (No auto-calibration, no seam detection)
 * 
 * Performs spherical warping and multi-band blending of 4 camera views
 * using pre-calibrated YAML parameters.
 * 
 * Features:
 * - Loads R and K matrices from YAML files
 * - Spherical projection warping
 * - Full overlap masks (no seam detection needed)
 * - Multi-band blending for smooth transitions
 * - Gain compensation for exposure matching
 */
class SVStitcherSimple {
public:
    SVStitcherSimple();
    ~SVStitcherSimple();
    
    /**
     * @brief Initialize from pre-calibrated YAML files
     * @param calib_folder Folder containing Camparam*.yaml files
     * @param sample_frames Sample frames for initial setup
     * @return true if successful
     */
    bool initFromFiles(const std::string& calib_folder, 
                       const std::vector<cv::cuda::GpuMat>& sample_frames);
    
    /**
     * @brief Stitch frames from all cameras
     * @param frames Vector of 4 camera frames (GPU)
     * @param output Stitched output frame (GPU)
     * @return true if successful
     */
    bool stitch(const std::vector<cv::cuda::GpuMat>& frames, 
                cv::cuda::GpuMat& output);
    
    /**
     * @brief Recompute gain compensation (call periodically)
     * @param frames Vector of 4 camera frames
     */
    void recomputeGain(const std::vector<cv::cuda::GpuMat>& frames);
    
    /**
     * @brief Check if stitcher is initialized
     * @return true if ready to stitch
     */
    bool isInitialized() const { return is_init; }
    
private:
    /**
     * @brief Load calibration parameters from YAML files
     * @param folder Path to calibration folder
     * @return true if all files loaded successfully
     */
    bool loadCalibration(const std::string& folder);
    
    /**
     * @brief Setup spherical warp lookup tables
     * @return true if successful
     */
    bool setupWarpMaps();
    
    /**
     * @brief Create full overlap masks (no seam detection)
     * @param sample_frames Sample frames for mask generation
     * @return true if successful
     */
    bool createOverlapMasks(const std::vector<cv::cuda::GpuMat>& sample_frames);
    
    /**
     * @brief Setup output cropping/warping
     * @param folder Path to calibration folder
     * @return true if successful
     */
    bool setupOutputCrop(const std::string& folder);
    
    // Calibration data
    std::vector<cv::Mat> K_matrices;      // Intrinsic matrices (4)
    std::vector<cv::Mat> R_matrices;      // Rotation matrices (4)
    float focal_length;                   // Focal length (from YAML)
    
    // Warping
    std::vector<cv::cuda::GpuMat> warp_x_maps;  // X warp maps (4)
    std::vector<cv::cuda::GpuMat> warp_y_maps;  // Y warp maps (4)
    std::vector<cv::Point> warp_corners;        // Warp corner positions (4)
    std::vector<cv::Size> warp_sizes;           // Warped image sizes (4)
    
    // Masks (full overlap, no seam detection)
    std::vector<cv::cuda::GpuMat> blend_masks;  // Blend masks (4)
    
    // Blending
    std::shared_ptr<SVMultiBandBlender> blender;
    
    // Gain compensation
    std::shared_ptr<SVGainCompensator> gain_comp;
    
    // Output cropping
    cv::cuda::GpuMat crop_warp_x;              // Crop X map
    cv::cuda::GpuMat crop_warp_y;              // Crop Y map
    cv::Size output_size;                       // Final output size
    
    // State
    bool is_init;
    int num_cameras;
    float scale_factor;
};

#endif // SV_STITCHER_SIMPLE_HPP
