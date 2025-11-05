CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
TARGET = camera_capture

# Get GStreamer, GLib, and yaml-cpp flags
GSTREAMER_CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0 yaml-cpp)
GSTREAMER_LIBS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 yaml-cpp)

# Source files
SOURCES = camera_capture.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(GSTREAMER_LIBS) -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(GSTREAMER_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
