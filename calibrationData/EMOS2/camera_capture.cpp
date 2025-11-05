#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <yaml-cpp/yaml.h>

struct CameraConfig {
    std::string address;
    int port;
};

class CameraCapture {
private:
    GstElement *pipeline;
    GstElement *appsink;
    GMainLoop *loop;
    std::string save_folder;
    int capture_count;
    std::atomic<bool> capture_requested;
    std::atomic<bool> running;
    CameraConfig config;

    struct termios orig_termios;

    void resetTerminalMode() {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }

    void setRawMode() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &orig_termios);
        raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    bool createFolder(const std::string& folder) {
        struct stat st;
        if (stat(folder.c_str(), &st) != 0) {
            if (mkdir(folder.c_str(), 0755) == 0) {
                std::cout << "Created folder: " << folder << std::endl;
                return true;
            } else {
                std::cerr << "Error: Could not create folder: " << folder << std::endl;
                return false;
            }
        }
        return true;
    }

    static gboolean busCallback(GstBus *bus, GstMessage *msg, gpointer data) {
        CameraCapture *capture = static_cast<CameraCapture*>(data);
        
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_EOS:
                std::cout << "\nEnd of stream" << std::endl;
                capture->stop();
                break;
                
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr << "Error: " << err->message << std::endl;
                if (debug_info) {
                    std::cerr << "Debug info: " << debug_info << std::endl;
                    g_free(debug_info);
                }
                g_error_free(err);
                capture->stop();
                break;
            }
            
            default:
                break;
        }
        
        return TRUE;
    }

    static GstFlowReturn newSampleCallback(GstAppSink *appsink, gpointer data) {
        CameraCapture *capture = static_cast<CameraCapture*>(data);
        
        if (!capture->capture_requested) {
            return GST_FLOW_OK;
        }

        GstSample *sample = gst_app_sink_pull_sample(appsink);
        if (sample) {
            capture->saveSample(sample);
            gst_sample_unref(sample);
            capture->capture_requested = false;
        }
        
        return GST_FLOW_OK;
    }

    void saveSample(GstSample *sample) {
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            capture_count++;
            
            std::string timestamp = getCurrentTimestamp();
            std::stringstream filename_ss;
            filename_ss << save_folder << "/capture_" << timestamp 
                        << "_" << std::setfill('0') << std::setw(4) << capture_count << ".jpg";
            std::string filename = filename_ss.str();

            // Write JPEG data to file
            std::ofstream file(filename, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(map.data), map.size);
                file.close();
                std::cout << "✓ Image " << capture_count << " saved: " << filename << std::endl;
            } else {
                std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
            }

            gst_buffer_unmap(buffer, &map);
        }
    }

public:
    CameraCapture(const std::string& folder, const CameraConfig& cfg) 
        : pipeline(nullptr), appsink(nullptr), loop(nullptr), 
          save_folder(folder), capture_count(0), 
          capture_requested(false), running(false), config(cfg) {
    }

    ~CameraCapture() {
        cleanup();
    }

    bool initialize() {
        if (!createFolder(save_folder)) {
            return false;
        }

        // Create pipeline with appsink for capture
        std::stringstream pipeline_ss;
        pipeline_ss << "udpsrc address=" << config.address << " port=" << config.port << " ! "
                    << "application/x-rtp,encoding-name=H264,payload=96 ! "
                    << "rtpjitterbuffer ! "
                    << "rtph264depay ! "
                    << "h264parse ! "
                    << "nvv4l2decoder ! "
                    << "tee name=t "
                    << "t. ! queue ! nvvidconv ! autovideosink "
                    << "t. ! queue ! nvvidconv ! video/x-raw,format=I420 ! "
                    << "jpegenc ! appsink name=appsink emit-signals=true max-buffers=1 drop=true";

        std::string pipeline_str = pipeline_ss.str();
        
        GError *error = nullptr;
        pipeline = gst_parse_launch(pipeline_str.c_str(), &error);
        
        if (error) {
            std::cerr << "Pipeline parsing error: " << error->message << std::endl;
            g_error_free(error);
            return false;
        }

        // Get appsink element
        appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
        if (!appsink) {
            std::cerr << "Error: Could not get appsink element" << std::endl;
            return false;
        }

        // Connect callback for new samples
        g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, NULL);
        g_signal_connect(appsink, "new-sample", G_CALLBACK(newSampleCallback), this);

        // Set up bus
        GstBus *bus = gst_element_get_bus(pipeline);
        gst_bus_add_watch(bus, busCallback, this);
        gst_object_unref(bus);

        return true;
    }

    void captureImage() {
        if (capture_requested) {
            std::cout << "Already capturing, please wait..." << std::endl;
            return;
        }

        std::cout << "📸 Requesting capture..." << std::endl;
        capture_requested = true;
    }

    void start() {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "Starting camera stream..." << std::endl;
        std::cout << "Camera: " << config.address << ":" << config.port << std::endl;
        std::cout << "Saving images to: " << save_folder << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "\nControls:" << std::endl;
        std::cout << "  Press 'c' to capture image" << std::endl;
        std::cout << "  Press 'q' to quit\n" << std::endl;

        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Error: Unable to set pipeline to playing state" << std::endl;
            return;
        }

        running = true;
        setRawMode();

        loop = g_main_loop_new(nullptr, FALSE);

        std::thread keyboard_thread(&CameraCapture::keyboardMonitor, this);

        g_main_loop_run(loop);

        running = false;
        keyboard_thread.join();
        resetTerminalMode();
    }

    void keyboardMonitor() {
        while (running) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 'c' || c == 'C') {
                    captureImage();
                } else if (c == 'q' || c == 'Q') {
                    std::cout << "\nQuitting..." << std::endl;
                    stop();
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void stop() {
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
        }
        if (loop && g_main_loop_is_running(loop)) {
            g_main_loop_quit(loop);
        }
        running = false;
    }

    void cleanup() {
        if (pipeline) {
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
        if (loop) {
            g_main_loop_unref(loop);
            loop = nullptr;
        }
    }

    int getCaptureCount() const {
        return capture_count;
    }

    std::string getSaveFolder() const {
        return save_folder;
    }
};

bool loadConfig(const std::string& config_file, CameraConfig& config) {
    try {
        YAML::Node yaml_config = YAML::LoadFile(config_file);
        
        if (yaml_config["camera"]) {
            config.address = yaml_config["camera"]["address"].as<std::string>();
            config.port = yaml_config["camera"]["port"].as<int>();
            return true;
        } else {
            std::cerr << "Error: 'camera' section not found in config file" << std::endl;
            return false;
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "Error parsing YAML config: " << e.what() << std::endl;
        return false;
    }
}

bool createDefaultConfig(const std::string& config_file) {
    std::ofstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not create config file" << std::endl;
        return false;
    }

    file << "# Camera Configuration\n";
    file << "camera:\n";
    file << "  address: \"192.168.45.3\"\n";
    file << "  port: 5020\n";
    file.close();

    std::cout << "Created default config file: " << config_file << std::endl;
    std::cout << "Please edit the file and run again.\n" << std::endl;
    return true;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    std::cout << std::string(50, '=') << std::endl;
    std::cout << "GStreamer Image Capture Tool (C++)" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    // Load configuration
    std::string config_file = "camera_config.yaml";
    CameraConfig config;

    struct stat st;
    if (stat(config_file.c_str(), &st) != 0) {
        std::cout << "\nConfig file not found. Creating default configuration..." << std::endl;
        if (!createDefaultConfig(config_file)) {
            return 1;
        }
        return 0;
    }

    if (!loadConfig(config_file, config)) {
        std::cerr << "Failed to load configuration from: " << config_file << std::endl;
        return 1;
    }

    std::cout << "\nLoaded configuration:" << std::endl;
    std::cout << "  Camera Address: " << config.address << std::endl;
    std::cout << "  Camera Port: " << config.port << std::endl;

    // Ask for folder name
    std::string folder_name;
    std::cout << "\nEnter folder name to save images: ";
    std::getline(std::cin, folder_name);

    folder_name.erase(0, folder_name.find_first_not_of(" \t\n\r"));
    folder_name.erase(folder_name.find_last_not_of(" \t\n\r") + 1);

    if (folder_name.empty()) {
        folder_name = "captured_images";
        std::cout << "No folder name provided. Using 'captured_images' as default." << std::endl;
    }

    // Create and initialize camera capture
    CameraCapture capture(folder_name, config);
    
    if (!capture.initialize()) {
        std::cerr << "Failed to initialize camera capture" << std::endl;
        return 1;
    }

    capture.start();

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Session Summary:" << std::endl;
    std::cout << "Total images captured: " << capture.getCaptureCount() << std::endl;
    std::cout << "Images saved in: " << capture.getSaveFolder() << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    return 0;
}
