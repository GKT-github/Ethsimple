# Simple Surround View System

A streamlined 4-camera 360° surround view system for Jetson Nano with spherical bowl rendering.

## Features

- ✅ 4 H.264 Ethernet cameras (120° FOV each)
- ✅ Spherical warping and stitching
- ✅ Multi-band blending for seamless transitions
- ✅ Gain compensation for exposure matching
- ✅ Real-time OpenGL spherical bowl rendering
- ✅ 3D car model overlay
- ✅ Fixed camera setup (no auto-calibration needed)
- ✅ HD output (1920x1080)

## System Requirements

### Hardware
- Jetson Nano (4GB recommended)
- 4x Ethernet cameras (H.264 streaming)
- Network switch for camera connections

### Software
- Ubuntu 18.04/20.04
- CUDA 10.2+
- OpenCV 4.x with CUDA support
- GStreamer 1.x
- OpenGL 3.2+
- GLFW3

## Camera Setup

```
Camera Layout (Top View):

        [Front]
        Camera 0
        0° Yaw
           |
Left ------+------ Right
Camera 1   |    Camera 3
90° Yaw    |    270° Yaw
           |
        [Rear]
        Camera 2
        180° Yaw

All cameras: 20° pitch (tilted down)
             120° horizontal FOV
```

### Camera Network Configuration

| Camera | Position | IP Address      | Port | Yaw  | Pitch |
|--------|----------|-----------------|------|------|-------|
| 0      | Front    | 192.168.45.10   | 5020 | 0°   | 20°   |
| 1      | Left     | 192.168.45.11   | 5021 | 90°  | 20°   |
| 2      | Rear     | 192.168.45.12   | 5022 | 180° | 20°   |
| 3      | Right    | 192.168.45.13   | 5023 | 270° | 20°   |

## Installation

### 1. Install Dependencies

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install build tools
sudo apt install -y build-essential cmake pkg-config

# Install OpenCV with CUDA (if not already installed)
# Follow: https://docs.opencv.org/4.x/d6/d15/tutorial_building_tegra_cuda.html

# Install GStreamer
sudo apt install -y libgstreamer1.0-dev \
                    libgstreamer-plugins-base1.0-dev \
                    gstreamer1.0-plugins-good \
                    gstreamer1.0-plugins-bad

# Install OpenGL and GLFW
sudo apt install -y libglfw3-dev libglm-dev \
                    libgles2-mesa-dev libegl1-mesa-dev

# Install Python for calibration generation
sudo apt install -y python3-opencv python3-numpy
```

### 2. Clone and Build

```bash
# Clone repository
git clone <repository-url>
cd SurroundViewSimple

# Generate calibration files
cd camparameters
python3 ../generate_calibration.py
cd ..

# Build
mkdir build && cd build
cmake ..
make -j4

# The executable will be: build/SurroundViewSimple
```

## Project Structure

```
SurroundViewSimple/
├── README.md                    # This file
├── CMakeLists.txt              # Build configuration
├── generate_calibration.py     # Calibration YAML generator
│
├── include/                    # Header files
│   ├── SVConfig.hpp           # System configuration
│   ├── SVAppSimple.hpp        # Main application
│   ├── SVStitcherSimple.hpp   # Stitching engine
│   ├── SVRenderSimple.hpp     # OpenGL renderer
│   ├── SVEthernetCamera.hpp   # Camera interface
│   ├── SVBlender.hpp          # Multi-band blending
│   ├── SVGainCompensator.hpp  # Gain compensation
│   ├── Bowl.hpp               # Bowl geometry
│   ├── OGLShader.hpp          # Shader utilities
│   ├── Model.hpp              # 3D model loader
│   └── Mesh.hpp               # Mesh utilities
│
├── src/                       # Source files
│   ├── main.cpp
│   ├── SVAppSimple.cpp
│   ├── SVStitcherSimple.cpp
│   ├── SVRenderSimple.cpp
│   ├── SVEthernetCamera.cpp
│   ├── SVBlender.cpp
│   ├── SVGainCompensator.cpp
│   ├── Bowl.cpp
│   ├── OGLShader.cpp
│   ├── Model.cpp
│   └── Mesh.cpp
│
├── cusrc/                     # CUDA kernels
│   ├── kernelblend.cu
│   └── kernelgain.cu
│
├── shaders/                   # GLSL shaders
│   ├── surroundshadervert.glsl
│   ├── surroundshaderfrag.glsl
│   ├── carshadervert.glsl
│   └── carshaderfrag.glsl
│
├── models/                    # 3D models
│   └── Dodge Challenger SRT Hellcat 2015.obj
│
├── camparameters/            # Calibration files (generated)
│   ├── Camparam0.yaml
│   ├── Camparam1.yaml
│   ├── Camparam2.yaml
│   ├── Camparam3.yaml
│   └── corner_warppts.yaml
│
└── 3dparty/                  # Third-party libraries
    └── glm/                  # GLM math library
```

## Usage

### Generate Calibration Files

```bash
cd camparameters
python3 ../generate_calibration.py
```

This creates:
- `Camparam0.yaml` - `Camparam3.yaml` (camera parameters)
- `corner_warppts.yaml` (output crop configuration)

### Run the System

```bash
# From build directory
./SurroundViewSimple ../camparameters

# Or specify custom calibration folder
./SurroundViewSimple /path/to/calibration/folder
```

### Controls

- **ESC or Ctrl+C**: Exit application
- No mouse controls (fixed camera view)

## Configuration

### Modify Camera Parameters

Edit `generate_calibration.py`:

```python
# Image resolution
image_width = 1280
image_height = 800

# Field of view
horizontal_fov = 120  # degrees

# Camera tilt
pitch_down = 20  # degrees

# Camera network addresses
cameras = [
    (0, "Front", 0,   20, 0, "192.168.45.10", 5020),
    (1, "Left",  90,  20, 0, "192.168.45.11", 5021),
    (2, "Rear",  180, 20, 0, "192.168.45.12", 5022),
    (3, "Right", 270, 20, 0, "192.168.45.13", 5023),
]
```

Re-run the script to regenerate YAML files.

### Modify System Parameters

Edit `include/SVConfig.hpp`:

```cpp
// Camera configuration
#define NUM_CAMERAS 4
#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 800

// Output configuration
#define OUTPUT_WIDTH 1920
#define OUTPUT_HEIGHT 1080

// Blending bands (more = smoother, slower)
#define NUM_BLEND_BANDS 5

// Processing scale (lower = faster, lower quality)
#define PROCESS_SCALE 0.65f
```

### Camera Network Addresses

Edit `include/SVConfig.hpp`:

```cpp
static const CameraConfig CAMERA_CONFIGS[NUM_CAMERAS] = {
    {"Front", "192.168.45.10", 5020},
    {"Left",  "192.168.45.11", 5021},
    {"Rear",  "192.168.45.12", 5022},
    {"Right", "192.168.45.13", 5023}
};
```

## Troubleshooting

### Cameras Not Connecting

```bash
# Check network connectivity
ping 192.168.45.10
ping 192.168.45.11
ping 192.168.45.12
ping 192.168.45.13

# Check if streams are available
gst-launch-1.0 udpsrc port=5020 ! application/x-rtp ! rtph264depay ! h264parse ! queue ! fakesink
```

### Low Frame Rate

1. **Reduce output resolution** in `SVConfig.hpp`
2. **Reduce blend bands** (try 3 instead of 5)
3. **Increase process scale** (try 0.8 instead of 0.65)
4. **Disable car model** (comment out in SVAppSimple.cpp)

### Build Errors

```bash
# Check OpenCV installation
pkg-config --modversion opencv4

# Check CUDA installation
nvcc --version

# Verify all dependencies
sudo apt install -y libgstreamer1.0-dev \
                    libgstreamer-plugins-base1.0-dev \
                    libglfw3-dev libglm-dev
```

### Stitching Issues

1. **Verify calibration files** exist in `camparameters/`
2. **Check camera FOV** - 120° recommended for good overlap
3. **Verify camera angles** - must be 90° apart (0°, 90°, 180°, 270°)
4. **Check camera tilt** - all should have same pitch (20° down)

## Performance

Expected performance on Jetson Nano:
- **Resolution**: 1920x1080 output
- **Frame Rate**: 15-20 FPS
- **Latency**: <100ms
- **GPU Memory**: ~2GB
- **CPU Usage**: ~30%

## What Was Removed from Original

This simplified version removes:
- ❌ Auto-calibration (use manual YAML files)
- ❌ Seam detection (uses full overlap masks)
- ❌ Undistortion (assumes cameras corrected or minimal distortion)
- ❌ Pedestrian detection
- ❌ Tone mapping
- ❌ Mouse camera controls
- ❌ Rear view splitting (4 cameras instead of 5 views)
- ❌ Multiple display modes

## License

[Your license here]

## Support

For issues and questions:
- Check the troubleshooting section
- Review camera network configuration
- Verify calibration files are generated correctly

## Acknowledgments

Based on the EtherSurround360 project with significant simplifications for embedded deployment.
