#include "SVAppSimple.hpp"
#include <iostream>
#include <csignal>

volatile bool g_running = true;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received." << std::endl;
    g_running = false;
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "Simple Surround View System" << std::endl;
    std::cout << "4 Cameras - 120° FOV - Spherical Bowl" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Get calibration folder from command line or use default
    std::string calib_folder = "camparameters";
    if (argc > 1) {
        calib_folder = argv[1];
    }
    
    std::cout << "\nCalibration folder: " << calib_folder << std::endl;
    
    // Create application
    SVAppSimple app;
    
    // Initialize
    std::cout << "\n--- Initialization Phase ---" << std::endl;
    if (!app.init(calib_folder)) {
        std::cerr << "\nERROR: Failed to initialize application" << std::endl;
        return -1;
    }
    
    std::cout << "\n--- Running... (Press Ctrl+C to stop) ---" << std::endl;
    
    // Run main loop
    app.run();
    
    // Cleanup
    std::cout << "\n--- Shutting down ---" << std::endl;
    app.stop();
    
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
