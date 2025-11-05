# Quick Start Guide - Simple Surround View

Get your 4-camera surround view system running in minutes!

## Prerequisites

- Jetson Nano with Ubuntu 18.04/20.04
- 4x H.264 Ethernet cameras configured on network
- OpenCV 4.x with CUDA support
- GStreamer, OpenGL, GLFW installed

## 5-Minute Setup

### Step 1: Prepare Directory Structure

```bash
# Create project directory
mkdir ~/SurroundViewSimple
cd ~/SurroundViewSimple

# Create subdirectories
mkdir -p include src cusrc shaders models camparameters 3dparty/glm build
```

### Step 2: Place Files

**Copy these NEW files from the package:**

```bash
# Root directory
cp /path/to/package/README.md ./
cp /path/to/package/CMakeLists.txt ./
cp /path/to/package/generate_calibration.py ./

# Headers (include/)
cp /path/to/package/SVConfig.hpp ./include/
cp /path/to/package/SVAppSimple.hpp ./include/
cp /path/to/package/SVStitcherSimple.hpp ./include/
cp /path/to/package/SVRenderSimple.hpp ./include/

# Source (src/)
cp /path/to/package/main.cpp ./src/
cp /path/to/package/SVAppSimple.cpp ./src/
cp /path/to/package/SVStitcherSimple.cpp ./src/
cp /path/to/package/SVRenderSimple.cpp ./src/
```

**Copy these EXISTING files from your original project:**

```bash
# From your original Surround-View project
ORIGINAL="/path/to/original/Surround-View-master"

# Headers
cp $ORIGINAL/include/SVEthernetCamera.hpp ./include/
cp $ORIGINAL/include/SVBlender.hpp ./include/
cp $ORIGINAL/include/SVGainCompensator.hpp ./include/
cp $ORIGINAL/include/Bowl.hpp ./include/
cp $ORIGINAL/include/OGLShader.hpp ./include/
cp $ORIGINAL/include/Model.hpp ./include/
cp $ORIGINAL/include/Mesh.hpp ./include/

# Source
cp $ORIGINAL/src/SVEthernetCamera.cpp ./src/
cp $ORIGINAL/src/SVBlender.cpp ./src/
cp $ORIGINAL/src/SVGainCompensator.cpp ./src/
cp $ORIGINAL/src/Bowl.cpp ./src/
cp $ORIGINAL/src/OGLShader.cpp ./src/
cp $ORIGINAL/src/Model.cpp ./src/
cp $ORIGINAL/src/Mesh.cpp ./src/

# CUDA kernels
cp $ORIGINAL/cusrc/*.cu ./cusrc/

# Shaders
cp $ORIGINAL/shaders/*.glsl ./shaders/

# Models
cp -r $ORIGINAL/models/* ./models/

# GLM library
cp -r $ORIGINAL/3dparty/glm ./3dparty/
```

### Step 3: Configure Camera Network

Edit `include/SVConfig.hpp`:

```cpp
static const CameraConfig CAMERA_CONFIGS[NUM_CAMERAS] = {
    {"Front", "192.168.45.10", 5020},  // ← Change these IPs
    {"Left",  "192.168.45.11", 5021},  // ← to match your
    {"Rear",  "192.168.45.12", 5022},  // ← camera network
    {"Right", "192.168.45.13", 5023}   // ← addresses
};
```

### Step 4: Generate Calibration Files

```bash
cd camparameters
python3 ../generate_calibration.py
cd ..
```

**Verify files created:**
```bash
ls camparameters/
# Should show: Camparam0.yaml, Camparam1.yaml, Camparam2.yaml, 
#              Camparam3.yaml, corner_warppts.yaml
```

### Step 5: Build

```bash
cd build
cmake ..
make -j4
```

**Expected output:**
```
[  5%] Building CXX object CMakeFiles/SurroundViewSimple.dir/src/main.cpp.o
[ 10%] Building CXX object CMakeFiles/SurroundViewSimple.dir/src/SVAppSimple.cpp.o
...
[100%] Linking CXX executable SurroundViewSimple
[100%] Built target SurroundViewSimple
```

### Step 6: Test Cameras

```bash
# Test network connectivity
ping -c 3 192.168.45.10
ping -c 3 192.168.45.11
ping -c 3 192.168.45.12
ping -c 3 192.168.45.13
```

### Step 7: Run!

```bash
./SurroundViewSimple ../camparameters
```

---

## Expected Startup Sequence

```
========================================
Simple Surround View System
4 Cameras - 120° FOV - Spherical Bowl
========================================

Calibration folder: ../camparameters

--- Initialization Phase ---
Initializing Simple Surround View System
========================================

[1/4] Initializing camera source...
  ✓ Cameras initialized
  ✓ Camera streams started

[2/4] Waiting for camera frames...
  ✓ Received valid frames from all 4 cameras
    Camera 0: [1280 x 800]
    Camera 1: [1280 x 800]
    Camera 2: [1280 x 800]
    Camera 3: [1280 x 800]

[3/4] Initializing stitcher...
Loading calibration files...
  ✓ Camera 0: ../camparameters/Camparam0.yaml
  ✓ Camera 1: ../camparameters/Camparam1.yaml
  ✓ Camera 2: ../camparameters/Camparam2.yaml
  ✓ Camera 3: ../camparameters/Camparam3.yaml
  Focal length: 554.26 pixels
Creating spherical warp maps...
  ✓ Camera 0: corner=[...], size=[...]
  ...
Multi-band blender initialized (5 bands)
Gain compensator initialized
✓ Stitcher initialization complete!
  ✓ Stitcher ready

[4/4] Initializing renderer...
OpenGL initialized
Bowl shaders loaded
Bowl geometry created: ... indices
Car model loaded
  ✓ Renderer ready

========================================
✓ System Initialization Complete!
========================================

Configuration:
  Cameras: 4
  Input resolution: 1280x800
  Output resolution: 1920x1080
  Blend bands: 5
  Process scale: 0.65

Press Ctrl+C to exit

--- Running... (Press Ctrl+C to stop) ---
Starting main loop...
FPS: 18.5
FPS: 19.2
...
```

---

## Troubleshooting Quick Fixes

### No cameras found
```bash
# Check network
ip addr show

# Set static IP if needed
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.45.100/24
sudo nmcli con mod "Wired connection 1" ipv4.method manual
sudo nmcli con up "Wired connection 1"
```

### Cameras connect but no frames
```bash
# Test H.264 stream directly
gst-launch-1.0 udpsrc port=5020 caps="application/x-rtp" ! \
    rtph264depay ! h264parse ! queue ! fakesink

# If this works, check your camera streaming configuration
```

### Build errors
```bash
# Install missing dependencies
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    libopencv-dev libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libglfw3-dev libglm-dev
```

### Low FPS (< 10)
Edit `include/SVConfig.hpp`:
```cpp
#define OUTPUT_WIDTH 1280      // Lower from 1920
#define OUTPUT_HEIGHT 720      // Lower from 1080
#define NUM_BLEND_BANDS 3      // Lower from 5
#define PROCESS_SCALE 0.75f    // Higher from 0.65
```
Rebuild and run again.

### Stitching seams visible
1. Check camera mounting: all must be 90° apart
2. Verify pitch: all cameras tilted down same amount (20°)
3. Check overlap: cameras need ~30° overlap minimum

---

## Performance Targets

### Jetson Nano (10W mode):
- **Resolution**: 1920x1080
- **FPS**: 15-20
- **Latency**: < 100ms

### Jetson Nano (optimized):
- **Resolution**: 1280x720
- **FPS**: 25-30
- **Latency**: < 80ms

---

## Customization

### Change camera angles
Edit `generate_calibration.py`:
```python
cameras = [
    (0, "Front", 0,   20, 0, "192.168.45.10", 5020),  # Change angles here
    (1, "Left",  90,  20, 0, "192.168.45.11", 5021),  # Yaw, Pitch, Roll
    (2, "Rear",  180, 20, 0, "192.168.45.12", 5022),
    (3, "Right", 270, 20, 0, "192.168.45.13", 5023),
]
```
Re-run script and rebuild.

### Change FOV
Edit `generate_calibration.py`:
```python
horizontal_fov = 110  # Change from 120 to your camera's FOV
```
Re-run script and rebuild.

### Disable car model
Edit `src/SVAppSimple.cpp`, comment out lines ~101-108:
```cpp
// if (!renderer->init(
//     "models/Dodge Challenger SRT Hellcat 2015.obj",
//     ...
// )) {
//     ...
// }
```

---

## Next Steps

1. ✅ Verify system runs at target FPS
2. ✅ Adjust camera positions if seams visible
3. ✅ Fine-tune gain compensation interval if needed
4. ✅ Test in different lighting conditions
5. ✅ Monitor GPU temperature and throttling

---

## File Checklist

Before building, verify you have:

```
SurroundViewSimple/
├── ✅ README.md
├── ✅ CMakeLists.txt
├── ✅ generate_calibration.py
├── include/
│   ├── ✅ SVConfig.hpp
│   ├── ✅ SVAppSimple.hpp
│   ├── ✅ SVStitcherSimple.hpp
│   ├── ✅ SVRenderSimple.hpp
│   ├── ✅ SVEthernetCamera.hpp
│   ├── ✅ SVBlender.hpp
│   ├── ✅ SVGainCompensator.hpp
│   ├── ✅ Bowl.hpp
│   ├── ✅ OGLShader.hpp
│   ├── ✅ Model.hpp
│   └── ✅ Mesh.hpp
├── src/
│   ├── ✅ main.cpp
│   ├── ✅ SVAppSimple.cpp
│   ├── ✅ SVStitcherSimple.cpp
│   ├── ✅ SVRenderSimple.cpp
│   ├── ✅ SVEthernetCamera.cpp
│   ├── ✅ SVBlender.cpp
│   ├── ✅ SVGainCompensator.cpp
│   ├── ✅ Bowl.cpp
│   ├── ✅ OGLShader.cpp
│   ├── ✅ Model.cpp
│   └── ✅ Mesh.cpp
├── cusrc/
│   ├── ✅ kernelblend.cu
│   └── ✅ kernelgain.cu
├── shaders/
│   ├── ✅ surroundshadervert.glsl
│   ├── ✅ surroundshaderfrag.glsl
│   ├── ✅ carshadervert.glsl
│   └── ✅ carshaderfrag.glsl
├── models/
│   └── ✅ [car model .obj file]
├── camparameters/
│   ├── ✅ Camparam0.yaml (generated)
│   ├── ✅ Camparam1.yaml (generated)
│   ├── ✅ Camparam2.yaml (generated)
│   ├── ✅ Camparam3.yaml (generated)
│   └── ✅ corner_warppts.yaml (generated)
└── 3dparty/glm/
    └── ✅ [GLM headers]
```

---

## Success Criteria

Your system is working correctly if:

✅ All 4 cameras connect and stream
✅ Stitched view displays in OpenGL window
✅ No visible seams between cameras
✅ Car model renders correctly in center
✅ FPS >= 15 on Jetson Nano
✅ No GPU memory errors
✅ Smooth frame updates (no stuttering)

---

## Support

If you encounter issues:

1. Check this guide's troubleshooting section
2. Review README.md detailed documentation
3. Verify PACKAGE_STRUCTURE.md for file organization
4. Check camera network configuration
5. Validate calibration YAML files

---

**You're ready to go! Happy stitching! 🚗📷**
