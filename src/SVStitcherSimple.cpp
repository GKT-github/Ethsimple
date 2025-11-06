#include "SVStitcherSimple.hpp"
#include <opencv2/calib3d.hpp>
#include <opencv2/stitching/detail/warpers.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <iostream>

SVStitcherSimple::SVStitcherSimple() 
    : is_init(false), num_cameras(NUM_CAMERAS), scale_factor(PROCESS_SCALE) {
}

SVStitcherSimple::~SVStitcherSimple() {
}

bool SVStitcherSimple::initFromFiles(const std::string& calib_folder,
                                     const std::vector<cv::cuda::GpuMat>& sample_frames) {
    if (is_init) {
        std::cerr << "Stitcher already initialized" << std::endl;
        return false;
    }
    
    if (sample_frames.size() != num_cameras) {
        std::cerr << "Wrong number of frames: " << sample_frames.size() 
                  << " (expected " << num_cameras << ")" << std::endl;
        return false;
    }
    
    std::cout << "Initializing stitcher..." << std::endl;
    std::cout << "  Calibration folder: " << calib_folder << std::endl;
    std::cout << "  Number of cameras: " << num_cameras << std::endl;
    std::cout << "  Scale factor: " << scale_factor << std::endl;
    
    // Load calibration files
    if (!loadCalibration(calib_folder)) {
        return false;
    }
    
    // Setup warp maps
    if (!setupWarpMaps()) {
        return false;
    }
    
    // Create full overlap masks (no seam detection)
    if (!createOverlapMasks(sample_frames)) {
        return false;
    }
    
    // Initialize blender
    blender = std::make_shared<SVMultiBandBlender>(NUM_BLEND_BANDS);
    blender->prepare(warp_corners, warp_sizes, blend_masks);
    
    std::cout << "Multi-band blender initialized (" << NUM_BLEND_BANDS << " bands)" << std::endl;
    
    // Initialize gain compensator
    gain_comp = std::make_shared<SVGainCompensator>(num_cameras);
    
    // Create sample warped frames for gain initialization
    std::vector<cv::cuda::GpuMat> warped_samples(num_cameras);
    for (int i = 0; i < num_cameras; i++) {
        cv::cuda::GpuMat scaled;
        cv::cuda::resize(sample_frames[i], scaled, cv::Size(),
                        scale_factor, scale_factor, cv::INTER_LINEAR);
        cv::cuda::remap(scaled, warped_samples[i],
                       warp_x_maps[i], warp_y_maps[i],
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    }
    
    gain_comp->init(warped_samples, warp_corners, blend_masks);
    
    std::cout << "Gain compensator initialized" << std::endl;
    
    // Setup output cropping
    if (!setupOutputCrop(calib_folder)) {
        return false;
    }
    
    is_init = true;
    std::cout << "✓ Stitcher initialization complete!" << std::endl;
    
    return true;
}

bool SVStitcherSimple::loadCalibration(const std::string& folder) {
    K_matrices.resize(num_cameras);
    R_matrices.resize(num_cameras);
    
    std::cout << "Loading calibration files..." << std::endl;
    
    for (int i = 0; i < num_cameras; i++) {
        std::string filename = folder + "/Camparam" + std::to_string(i) + ".yaml";
        
        cv::FileStorage fs(filename, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cerr << "ERROR: Failed to open: " << filename << std::endl;
            return false;
        }
        
        fs["FocalLength"] >> focal_length;
        fs["Intrisic"] >> K_matrices[i];
        fs["Rotation"] >> R_matrices[i];
        
        fs.release();
        
        std::cout << "  ✓ Camera " << i << ": " << filename << std::endl;
    }
    
    std::cout << "  Focal length: " << focal_length << " pixels" << std::endl;
    
    return true;
}

bool SVStitcherSimple::setupWarpMaps() {
    warp_x_maps.resize(num_cameras);
    warp_y_maps.resize(num_cameras);
    warp_corners.resize(num_cameras);
    warp_sizes.resize(num_cameras);
    
    // Create spherical warper    ////
    // cv::Ptr<cv::WarperCreator> warper_creator = cv::makePtr<cv::SphericalWarper>();
    // cv::Ptr<cv::detail::RotationWarper> warper = 
    //     warper_creator->create(static_cast<float>(scale_factor * focal_length));
    auto warper = cv::makePtr<cv::detail::SphericalWarper>(static_cast<float>(scale_factor * focal_length));
    std::cout << "Creating spherical warp maps..." << std::endl;
    
    cv::Size input_size(CAMERA_WIDTH, CAMERA_HEIGHT);
    cv::Size scaled_input(input_size.width * scale_factor, 
                          input_size.height * scale_factor);
    
    for (int i = 0; i < num_cameras; i++) {
        // Scale intrinsic matrix
        cv::Mat K_scaled = K_matrices[i].clone();
        K_scaled.at<float>(0, 0) *= scale_factor;  // fx
        K_scaled.at<float>(1, 1) *= scale_factor;  // fy
        K_scaled.at<float>(0, 2) *= scale_factor;  // cx
        K_scaled.at<float>(1, 2) *= scale_factor;  // cy
        
        // Build warp maps
        cv::Mat xmap, ymap;
        
        // Get corner and warped size
        cv::Mat dummy_warped;
        warp_corners[i] = warper->warp(
            cv::Mat::zeros(scaled_input, CV_8UC3),
            K_scaled, R_matrices[i],
            cv::INTER_LINEAR, cv::BORDER_REFLECT,
            dummy_warped
        );
        
        warp_sizes[i] = dummy_warped.size();
        
        // Build actual maps
        warper->buildMaps(scaled_input, K_scaled, R_matrices[i], xmap, ymap);
        
        // Upload to GPU
        warp_x_maps[i].upload(xmap);
        warp_y_maps[i].upload(ymap);
        
        std::cout << "  ✓ Camera " << i << ": corner=" << warp_corners[i] 
                  << ", size=" << warp_sizes[i] << std::endl;
    }
    
    return true;
}

bool SVStitcherSimple::createOverlapMasks(const std::vector<cv::cuda::GpuMat>& sample_frames) {
    blend_masks.resize(num_cameras);
    
    std::cout << "Creating full overlap masks..." << std::endl;
    
    // Create warper for mask generation ////
    // cv::Ptr<cv::WarperCreator> warper_creator = cv::makePtr<cv::SphericalWarper>();
    // cv::Ptr<cv::detail::RotationWarper> warper = 
    //     warper_creator->create(static_cast<float>(scale_factor * focal_length));
    auto warper = cv::makePtr<cv::detail::SphericalWarper>(static_cast<float>(scale_factor * focal_length));

    for (int i = 0; i < num_cameras; i++) {
        // Create full white mask for entire image
        cv::Size scaled_size(CAMERA_WIDTH * scale_factor, CAMERA_HEIGHT * scale_factor);
        cv::Mat full_mask(scaled_size, CV_8U, cv::Scalar(255));
        
        // Scale K matrix
        cv::Mat K_scaled = K_matrices[i].clone();
        K_scaled.at<float>(0, 0) *= scale_factor;
        K_scaled.at<float>(1, 1) *= scale_factor;
        K_scaled.at<float>(0, 2) *= scale_factor;
        K_scaled.at<float>(1, 2) *= scale_factor;
        
        // Warp mask
        cv::Mat warped_mask;
        warper->warp(full_mask, K_scaled, R_matrices[i],
                     cv::INTER_NEAREST, cv::BORDER_CONSTANT, warped_mask);
        
        // Upload to GPU
        blend_masks[i].upload(warped_mask);
        
        std::cout << "  ✓ Camera " << i << ": mask size=" << warped_mask.size() << std::endl;
    }
    
    return true;
}

bool SVStitcherSimple::setupOutputCrop(const std::string& folder) {
    std::string crop_file = folder + "/corner_warppts.yaml";
    
    cv::FileStorage fs(crop_file, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "Warning: Could not load " << crop_file << std::endl;
        std::cerr << "Using default HD output without cropping" << std::endl;
        output_size = cv::Size(OUTPUT_WIDTH, OUTPUT_HEIGHT);
        return true;  // Non-fatal, will use simple resize
    }
    
    cv::Point tl, tr, bl, br;
    fs["res_size"] >> output_size;
    fs["tl"] >> tl;
    fs["tr"] >> tr;
    fs["bl"] >> bl;
    fs["br"] >> br;
    fs.release();
    
    std::cout << "Output crop configuration loaded" << std::endl;
    std::cout << "  Output size: " << output_size << std::endl;
    std::cout << "  Crop corners: TL=" << tl << ", TR=" << tr 
              << ", BL=" << bl << ", BR=" << br << std::endl;
    
    // Create perspective transform
    std::vector<cv::Point2f> src_pts = {
        cv::Point2f(tl.x, tl.y),
        cv::Point2f(tr.x, tr.y),
        cv::Point2f(bl.x, bl.y),
        cv::Point2f(br.x, br.y)
    };
    
    std::vector<cv::Point2f> dst_pts = {
        cv::Point2f(0, 0),
        cv::Point2f(output_size.width, 0),
        cv::Point2f(0, output_size.height),
        cv::Point2f(output_size.width, output_size.height)
    };
    
    cv::Mat transform = cv::getPerspectiveTransform(src_pts, dst_pts);
    
    // Build GPU warp maps
    cv::cuda::buildWarpPerspectiveMaps(transform, false, output_size, 
                                        crop_warp_x, crop_warp_y);
    
    return true;
}

bool SVStitcherSimple::stitch(const std::vector<cv::cuda::GpuMat>& frames,
                               cv::cuda::GpuMat& output) {
    if (!is_init) {
        std::cerr << "ERROR: Stitcher not initialized" << std::endl;
        return false;
    }
    
    if (frames.size() != num_cameras) {
        std::cerr << "ERROR: Wrong number of frames: " << frames.size() << std::endl;
        return false;
    }
    
    // Warp and feed to blender
    for (int i = 0; i < num_cameras; i++) {
        // Resize to processing scale
        cv::cuda::GpuMat scaled_frame;
        cv::cuda::resize(frames[i], scaled_frame, cv::Size(), 
                        scale_factor, scale_factor, cv::INTER_LINEAR);
        
        // Apply gain compensation
        cv::cuda::GpuMat compensated_frame;
        gain_comp->apply(scaled_frame, compensated_frame, i);
        
        // Warp using pre-computed maps
        cv::cuda::GpuMat warped_frame;
        cv::cuda::remap(compensated_frame, warped_frame, 
                       warp_x_maps[i], warp_y_maps[i],
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        
        // Convert to 16-bit for blending
        cv::cuda::GpuMat warped_16s;
        warped_frame.convertTo(warped_16s, CV_16SC3);
        
        // Feed to blender
        blender->feed(warped_16s, blend_masks[i], i);
    }
    
    // Blend
    cv::cuda::GpuMat blended;
    blender->blend(blended, false);
    
    // Apply output crop/warp if configured
    if (!crop_warp_x.empty() && !crop_warp_y.empty()) {
        cv::cuda::remap(blended, output, crop_warp_x, crop_warp_y,
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    } else {
        // Simple resize to output resolution
        cv::cuda::resize(blended, output, output_size, 0, 0, cv::INTER_LINEAR);
    }
    
    return true;
}

void SVStitcherSimple::recomputeGain(const std::vector<cv::cuda::GpuMat>& frames) {
    if (!is_init || !gain_comp) {
        return;
    }
    
    // Create warped frames for gain computation
    std::vector<cv::cuda::GpuMat> warped_frames(num_cameras);
    
    for (int i = 0; i < num_cameras; i++) {
        cv::cuda::GpuMat scaled_frame;
        cv::cuda::resize(frames[i], scaled_frame, cv::Size(),
                        scale_factor, scale_factor, cv::INTER_LINEAR);
        
        cv::cuda::remap(scaled_frame, warped_frames[i],
                       warp_x_maps[i], warp_y_maps[i],
                       cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    }
    
    gain_comp->recompute(warped_frames, warp_corners, blend_masks);
    
    std::cout << "Gain compensation updated" << std::endl;
}
