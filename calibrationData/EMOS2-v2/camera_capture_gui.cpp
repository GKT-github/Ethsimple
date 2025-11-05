#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono>
#include <iomanip>
#include <sstream>
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
    GtkWidget *window;
    GtkWidget *capture_button;
    GtkWidget *status_label;
    GtkWidget *counter_label;
    std::string save_folder;
    int capture_count;
    std::atomic<bool> capture_requested;
    CameraConfig config;

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
                
                // Update GUI
                std::string status_text = "✓ Image saved: " + filename;
                gtk_label_set_text(GTK_LABEL(status_label), status_text.c_str());
                
                std::string counter_text = "Images captured: " + std::to_string(capture_count);
                gtk_label_set_text(GTK_LABEL(counter_label), counter_text.c_str());
                
                // Re-enable button
                gtk_widget_set_sensitive(capture_button, TRUE);
            } else {
                std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
                gtk_label_set_text(GTK_LABEL(status_label), "✗ Error: Could not save file");
                gtk_widget_set_sensitive(capture_button, TRUE);
            }

            gst_buffer_unmap(buffer, &map);
        }
    }

    static void onCaptureButtonClicked(GtkWidget *widget, gpointer data) {
        CameraCapture *capture = static_cast<CameraCapture*>(data);
        capture->captureImage();
    }

    static gboolean onWindowDelete(GtkWidget *widget, GdkEvent *event, gpointer data) {
        CameraCapture *capture = static_cast<CameraCapture*>(data);
        capture->stop();
        return FALSE;
    }

public:
    CameraCapture(const std::string& folder, const CameraConfig& cfg) 
        : pipeline(nullptr), appsink(nullptr), window(nullptr),
          capture_button(nullptr), status_label(nullptr), counter_label(nullptr),
          save_folder(folder), capture_count(0), 
          capture_requested(false), config(cfg) {
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

    void createGUI() {
        // Create main window
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "Camera Capture Control");
        gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
        gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
        g_signal_connect(window, "delete-event", G_CALLBACK(onWindowDelete), this);

        // Create vertical box container
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
        gtk_container_add(GTK_CONTAINER(window), vbox);

        // Add camera info label
        std::stringstream info_ss;
        info_ss << "Camera: " << config.address << ":" << config.port;
        GtkWidget *info_label = gtk_label_new(info_ss.str().c_str());
        gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

        // Add folder info label
        std::string folder_text = "Saving to: " + save_folder;
        GtkWidget *folder_label = gtk_label_new(folder_text.c_str());
        gtk_box_pack_start(GTK_BOX(vbox), folder_label, FALSE, FALSE, 0);

        // Add separator
        GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(vbox), separator1, FALSE, FALSE, 5);

        // Add capture button
        capture_button = gtk_button_new_with_label("📸 Capture Image");
        gtk_widget_set_size_request(capture_button, -1, 50);
        g_signal_connect(capture_button, "clicked", G_CALLBACK(onCaptureButtonClicked), this);
        gtk_box_pack_start(GTK_BOX(vbox), capture_button, FALSE, FALSE, 0);

        // Add counter label
        counter_label = gtk_label_new("Images captured: 0");
        gtk_box_pack_start(GTK_BOX(vbox), counter_label, FALSE, FALSE, 0);

        // Add separator
        GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(vbox), separator2, FALSE, FALSE, 5);

        // Add status label
        status_label = gtk_label_new("Ready to capture");
        gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(status_label), 50);
        gtk_box_pack_start(GTK_BOX(vbox), status_label, TRUE, TRUE, 0);

        // Show all widgets
        gtk_widget_show_all(window);
    }

    void captureImage() {
        if (capture_requested) {
            gtk_label_set_text(GTK_LABEL(status_label), "Already capturing, please wait...");
            return;
        }

        gtk_label_set_text(GTK_LABEL(status_label), "📸 Capturing...");
        gtk_widget_set_sensitive(capture_button, FALSE);
        capture_requested = true;
    }

    void start() {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "Starting camera stream..." << std::endl;
        std::cout << "Camera: " << config.address << ":" << config.port << std::endl;
        std::cout << "Saving images to: " << save_folder << std::endl;
        std::cout << std::string(50, '=') << std::endl;

        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Error: Unable to set pipeline to playing state" << std::endl;
            return;
        }

        std::cout << "\nControl window opened. Click 'Capture Image' button to take pictures." << std::endl;
        std::cout << "Close the control window to quit.\n" << std::endl;

        // Run GTK main loop
        gtk_main();
    }

    void stop() {
        std::cout << "\nStopping..." << std::endl;
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
        }
        gtk_main_quit();
    }

    void cleanup() {
        if (pipeline) {
            gst_object_unref(pipeline);
            pipeline = nullptr;
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
    // Initialize X11 threading support FIRST (before GTK/GStreamer)
    XInitThreads();
    
    // Initialize GStreamer and GTK
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    std::cout << std::string(50, '=') << std::endl;
    std::cout << "GStreamer Image Capture Tool (C++ with GUI)" << std::endl;
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

    // Create GUI
    capture.createGUI();

    // Start capture
    capture.start();

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Session Summary:" << std::endl;
    std::cout << "Total images captured: " << capture.getCaptureCount() << std::endl;
    std::cout << "Images saved in: " << capture.getSaveFolder() << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    return 0;
}
