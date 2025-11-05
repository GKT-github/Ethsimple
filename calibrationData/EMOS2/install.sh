#!/bin/bash

echo "=================================================="
echo "GStreamer Camera Capture - Installation Script"
echo "=================================================="
echo ""

# Check if running on Ubuntu/Debian
if [ -f /etc/debian_version ]; then
    echo "Detected Debian/Ubuntu system"
else
    echo "Warning: This script is designed for Debian/Ubuntu systems"
    echo "You may need to adjust package names for your distribution"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""
echo "Installing dependencies..."
echo ""

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

if [ $? -ne 0 ]; then
    echo ""
    echo "Error: Failed to install dependencies"
    exit 1
fi

echo ""
echo "Checking for NVIDIA GStreamer plugins..."
if dpkg -l | grep -q "gstreamer1.0-nvv4l2"; then
    echo "✓ NVIDIA GStreamer plugins already installed"
else
    echo "! NVIDIA GStreamer plugins not found"
    echo "  If you're on Jetson Nano, install with:"
    echo "  sudo apt install gstreamer1.0-nvv4l2 nvidia-l4t-gstreamer"
fi

echo ""
echo "Compiling camera_capture..."
echo ""

make clean 2>/dev/null
make

if [ $? -eq 0 ]; then
    echo ""
    echo "=================================================="
    echo "✓ Installation completed successfully!"
    echo "=================================================="
    echo ""
    echo "Next steps:"
    echo "1. Edit camera_config.yaml with your camera settings"
    echo "2. Run: ./camera_capture"
    echo ""
else
    echo ""
    echo "=================================================="
    echo "✗ Compilation failed"
    echo "=================================================="
    echo ""
    echo "Please check the error messages above"
    exit 1
fi
