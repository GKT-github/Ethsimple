#!/bin/bash

echo "Compiling Camera Capture with GUI..."

g++ -std=c++11 -o camera_capture_gui camera_capture_gui.cpp \
    $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gtk+-3.0) \
    -lyaml-cpp -lX11 -lpthread

if [ $? -eq 0 ]; then
    echo "✓ Compilation successful!"
    echo "Run with: ./camera_capture_gui"
else
    echo "✗ Compilation failed"
    exit 1
fi
