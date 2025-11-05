#include "SVAppSimple.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

SVAppSimple::SVAppSimple() : is_running(false) {
}

SVAppSimple::~SVAppSimple() {
    stop();
}

bool SVAppSimple::init(const std::string& calib_folder) {
    calibration_folder = calib_folder;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Initializing Simple Surround View System" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // ========================================
    // STEP 1: Initialize Camera Source
    // ========================================
    std::cout << "[1/4] Initializing camera source..." << std::endl;
    
    camera_source = std::make_shared<MultiCameraSource>();
    camera_source->setFrameSize(cv::Size(CAMERA_WIDTH, CAMERA_HEIGHT));
    
    // Initialize without undistortion
    if (camera_source->init("", cv::Size(CAMERA_WIDTH, CAMERA_HEIGHT), 
                            cv::Size(CAMERA_WIDTH, CAMERA_HEIGHT), false) < 0) {
        std::cerr << "ERROR: Failed to initialize cameras" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Cameras initialized" << std::endl;
    
    // Start camera streams
    if (!camera_source->startStream()) {
        std::cerr << "ERROR: Failed to start camera streams" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Camera streams started" << std::endl;
    
    // ========================================
    // STEP 2: Wait for Valid Frames
    // ========================================
    std::cout << "\n[2/4] Waiting for camera frames..." << std::endl;
    
    int attempts = 0;
    bool got_frames = false;
    
    while (attempts < 100 && !got_frames) {
        if (camera_source->capture(frames)) {
            bool all_valid = true;
            for (int i = 0; i < NUM_CAMERAS; i++) {
                if (frames[i].gpuFrame.empty()) {
                    all_valid = false;
                    break;
                }
            }
            
            if (all_valid) {
                got_frames = true;
                std::cout << "  ✓ Received valid frames from all " << NUM_CAMERAS 
                          << " cameras" << std::endl;
                
                // Print frame info
                for (int i = 0; i < NUM_CAMERAS; i++) {
                    std::cout << "    Camera " << i << ": " 
                              << frames[i].gpuFrame.size() << std::endl;
                }
            }
        }
        
        if (!got_frames) {
            std::this_thread::sleep_for(100ms);
            attempts++;
            if (attempts % 10 == 0) {
                std::cout << "  Still waiting for frames... (" << attempts << "/100)" << std::endl;
            }
        }
    }
    
    if (!got_frames) {
        std::cerr << "ERROR: Failed to get valid frames from cameras" << std::endl;
        return false;
    }
    
    // ========================================
    // STEP 3: Initialize Stitcher
    // ========================================
    std::cout << "\n[3/4] Initializing stitcher..." << std::endl;
    
    stitcher = std::make_shared<SVStitcherSimple>();
    
    std::vector<cv::cuda::GpuMat> sample_frames;
    for (int i = 0; i < NUM_CAMERAS; i++) {
        sample_frames.push_back(frames[i].gpuFrame);
    }
    
    if (!stitcher->initFromFiles(calibration_folder, sample_frames)) {
        std::cerr << "ERROR: Failed to initialize stitcher" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Stitcher ready" << std::endl;
    
    // ========================================
    // STEP 4: Initialize Renderer
    // ========================================
    std::cout << "\n[4/4] Initializing renderer..." << std::endl;
    
    renderer = std::make_shared<SVRenderSimple>(OUTPUT_WIDTH, OUTPUT_HEIGHT);
    
    if (!renderer->init(
        "models/Dodge Challenger SRT Hellcat 2015.obj",
        "shaders/surroundshadervert.glsl",
        "shaders/surroundshaderfrag.glsl",
        "shaders/carshadervert.glsl",
        "shaders/carshaderfrag.glsl")) {
        std::cerr << "ERROR: Failed to initialize renderer" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Renderer ready" << std::endl;
    
    // ========================================
    // Initialization Complete
    // ========================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ System Initialization Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Cameras: " << NUM_CAMERAS << std::endl;
    std::cout << "  Input resolution: " << CAMERA_WIDTH << "x" << CAMERA_HEIGHT << std::endl;
    std::cout << "  Output resolution: " << OUTPUT_WIDTH << "x" << OUTPUT_HEIGHT << std::endl;
    std::cout << "  Blend bands: " << NUM_BLEND_BANDS << std::endl;
    std::cout << "  Process scale: " << PROCESS_SCALE << std::endl;
    std::cout << "\nPress Ctrl+C to exit\n" << std::endl;
    
    is_running = true;
    return true;
}

void SVAppSimple::run() {
    if (!is_running) {
        std::cerr << "ERROR: System not initialized" << std::endl;
        return;
    }
    
    auto last_gain_update = std::chrono::steady_clock::now();
    const auto gain_update_interval = std::chrono::seconds(GAIN_UPDATE_INTERVAL);
    
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto last_fps_time = start_time;
    
    std::cout << "Starting main loop..." << std::endl;
    
    while (is_running && !renderer->shouldClose()) {
        // Capture frames
        if (!camera_source->capture(frames)) {
            std::cerr << "WARNING: Frame capture failed" << std::endl;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        
        // Validate all frames
        bool all_valid = true;
        for (int i = 0; i < NUM_CAMERAS; i++) {
            if (frames[i].gpuFrame.empty()) {
                std::cerr << "WARNING: Frame " << i << " is empty" << std::endl;
                all_valid = false;
                break;
            }
        }
        
        if (!all_valid) {
            std::this_thread::sleep_for(1ms);
            continue;
        }
        
        // Prepare frame vector for stitcher
        std::vector<cv::cuda::GpuMat> gpu_frames;
        for (int i = 0; i < NUM_CAMERAS; i++) {
            gpu_frames.push_back(frames[i].gpuFrame);
        }
        
        // Stitch
        if (!stitcher->stitch(gpu_frames, stitched_output)) {
            std::cerr << "WARNING: Stitching failed" << std::endl;
            continue;
        }
        
        // Periodic gain update
        auto now = std::chrono::steady_clock::now();
        if (now - last_gain_update >= gain_update_interval) {
            std::cout << "Updating gain compensation..." << std::endl;
            stitcher->recomputeGain(gpu_frames);
            last_gain_update = now;
        }
        
        // Render
        if (!renderer->render(stitched_output)) {
            std::cerr << "ERROR: Rendering failed" << std::endl;
            break;
        }
        
        // FPS calculation and display
        frame_count++;
        if (frame_count % 30 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_fps_time).count();
            
            if (elapsed > 0) {
                float fps = (30.0f * 1000.0f) / elapsed;
                std::cout << "FPS: " << fps << std::endl;
            }
            
            last_fps_time = now;
        }
        
        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(3ms);
    }
    
    std::cout << "\nMain loop exited" << std::endl;
}

void SVAppSimple::stop() {
    is_running = false;
    
    if (camera_source) {
        std::cout << "Stopping camera streams..." << std::endl;
        camera_source->stopStream();
    }
    
    std::cout << "System stopped" << std::endl;
}
