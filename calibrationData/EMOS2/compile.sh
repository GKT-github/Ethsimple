#!/bin/bash
g++ -std=c++11 -o camera_capture camera_capture.cpp \
    $(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0) \
    $(pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0) \
    -lyaml-cpp -lpthread

echo "Compilation complete!"

