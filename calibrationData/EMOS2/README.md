# GStreamer Camera Capture Tool (C++)

A C++ application for capturing images from an Ethernet camera using GStreamer, with keyboard controls.

## Features

- ✅ **YAML Configuration**: Camera IP and port loaded from config file
- ✅ Captures images from UDP/RTP H.264 stream
- ✅ Press 'c' to capture images on demand
- ✅ Press 'q' to quit
- ✅ Custom folder name for saving images
- ✅ Automatic timestamping
- ✅ Live video preview
- ✅ NVIDIA hardware decoding (nvv4l2decoder)
- ✅ Fixed capture issues with appsink implementation

## Prerequisites

### Install GStreamer and dependencies:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-tools \
    libglib2.0-dev \
    libyaml-cpp-dev \
    build-essential \
    cmake \
    pkg-config
```

**For NVIDIA Jetson (additional):**
```bash
sudo apt install -y \
    gstreamer1.0-nvv4l2 \
    nvidia-l4t-gstreamer
```

## Compilation

### Configuration File

Before running the application, you need a `camera_config.yaml` file. On first run without a config file, the application will create a default one:

```yaml
# Camera Configuration
camera:
  address: "192.168.45.3"
  port: 5020
```

**Edit this file to match your camera settings before running the application.**

### Option 1: Using Makefile (Recommended)

```bash
make
```

### Option 2: Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

### Option 3: Direct compilation

```bash
g++ -std=c++11 -o camera_capture camera_capture.cpp \
    $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 yaml-cpp) -lpthread
```

## Usage

**First run (to create default config):**

```bash
./camera_capture
```

This will create `camera_config.yaml`. Edit it with your camera settings:

```bash
nano camera_config.yaml
```

**Run the application:**

```bash
./camera_capture
```

You'll be prompted to enter a folder name for saving images. The application will then:
1. Create the folder (if it doesn't exist)
2. Start the video stream
3. Show live preview
4. Wait for keyboard commands

### Controls:

- **Press 'c'**: Capture an image
- **Press 'q'**: Quit the application

### Example session:

```
==================================================
GStreamer Image Capture Tool (C++)
==================================================

Loaded configuration:
  Camera Address: 192.168.45.3
  Camera Port: 5020

Enter folder name to save images: my_captures
Created folder: my_captures

==================================================
Starting camera stream...
Camera: 192.168.45.3:5020
Saving images to: my_captures
==================================================

Controls:
  Press 'c' to capture image
  Press 'q' to quit

📸 Requesting capture...
✓ Image 1 saved: my_captures/capture_20240130_143022_001_0001.jpg

📸 Requesting capture...
✓ Image 2 saved: my_captures/capture_20240130_143045_234_0002.jpg

Quitting...

==================================================
Session Summary:
Total images captured: 2
Images saved in: my_captures
==================================================
```

## Image Naming Convention

Images are saved with the following format:
```
capture_YYYYMMDD_HHMMSS_mmm_NNNN.jpg
```

Where:
- `YYYYMMDD`: Date
- `HHMMSS`: Time
- `mmm`: Milliseconds
- `NNNN`: Sequential capture number (4 digits)

Example: `capture_20240130_143022_001_0001.jpg`

## Customization

### Change camera IP/Port:

Edit `camera_config.yaml`:

```yaml
camera:
  address: "YOUR_IP"
  port: YOUR_PORT
```

### Change image format:

Replace `jpegenc` with other encoders:
- `pngenc` for PNG format
- Change file extension in the filename generation accordingly

### Adjust video sink:

Replace `autovideosink` with:
- `xvimagesink` for X11
- `waylandsink` for Wayland
- `fakesink` for no preview

## Troubleshooting

### Error: "Could not get pipeline elements"
- Ensure all GStreamer plugins are installed
- Verify NVIDIA GStreamer plugins (on Jetson)

### Error: "Pipeline parsing error"
- Check if camera is streaming
- Verify IP address and port
- Test with original gst-launch command first

### No video preview
- Check display server (X11/Wayland)
- Try different video sink (xvimagesink, waylandsink)

### Permission issues
- Ensure folder write permissions
- Run with appropriate user privileges

## Clean build

```bash
make clean
```

## Optional: Install system-wide

```bash
sudo make install
```

This installs the binary to `/usr/local/bin/`

## License

Free to use and modify.

## Author

Created for GStreamer-based camera capture applications.
