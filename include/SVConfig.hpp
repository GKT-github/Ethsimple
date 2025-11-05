#ifndef SV_CONFIG_HPP
#define SV_CONFIG_HPP

// ============================================================
// SYSTEM CONFIGURATION
// Simple Surround View - Centralized Configuration
// ============================================================

// ============================================================
// CAMERA CONFIGURATION
// ============================================================

// Number of cameras
#define NUM_CAMERAS 4

// Camera resolution
#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 800

// Camera network configuration
struct CameraConfig {
    const char* name;
    const char* ip;
    int port;
};

// Camera network addresses - MODIFY THESE FOR YOUR SETUP
static const CameraConfig CAMERA_CONFIGS[NUM_CAMERAS] = {
    {"Front", "192.168.45.10", 5020},  // Camera 0: Front (0° yaw)
    {"Left",  "192.168.45.11", 5021},  // Camera 1: Left (90° yaw)
    {"Rear",  "192.168.45.12", 5022},  // Camera 2: Rear (180° yaw)
    {"Right", "192.168.45.13", 5023}   // Camera 3: Right (270° yaw)
};

// ============================================================
// OUTPUT CONFIGURATION
// ============================================================

// Output resolution (HD)
#define OUTPUT_WIDTH 1920
#define OUTPUT_HEIGHT 1080

// ============================================================
// PROCESSING CONFIGURATION
// ============================================================

// Multi-band blending bands (5 = high quality, 3 = faster)
#define NUM_BLEND_BANDS 5

// Processing scale factor (0.65 = balanced quality/speed)
// Lower = faster but lower quality
// Higher = slower but higher quality
#define PROCESS_SCALE 0.65f

// Gain compensation update interval (seconds)
#define GAIN_UPDATE_INTERVAL 10

// ============================================================
// RENDERING CONFIGURATION
// ============================================================

// Bowl rendering parameters
#define BOWL_DISK_RADIUS 0.4f
#define BOWL_PARAB_RADIUS 0.55f
#define BOWL_HOLE_RADIUS 0.08f
#define BOWL_VERTICES 750

// Camera view parameters
#define CAMERA_FOV 45.0f
#define CAMERA_POSITION_Y 2.0f
#define CAMERA_POSITION_Z 5.0f

// ============================================================
// DEBUG OPTIONS
// ============================================================

// Uncomment to enable debug output
// #define DEBUG_TIMING
// #define DEBUG_FRAMES
// #define DEBUG_WARPING

#endif // SV_CONFIG_HPP
