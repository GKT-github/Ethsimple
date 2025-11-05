#include "SVRenderSimple.hpp"
#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>
#include <iostream>

SVRenderSimple::SVRenderSimple(int width, int height)
    : screen_width(width), screen_height(height), 
      window(nullptr), texture_id(0), pbo_id(0), is_init(false) {
    
    aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
}

SVRenderSimple::~SVRenderSimple() {
    if (texture_id) glDeleteTextures(1, &texture_id);
    if (pbo_id) glDeleteBuffers(1, &pbo_id);
    if (bowl_VAO) glDeleteVertexArrays(1, &bowl_VAO);
    if (bowl_VBO) glDeleteBuffers(1, &bowl_VBO);
    if (bowl_EBO) glDeleteBuffers(1, &bowl_EBO);
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool SVRenderSimple::init(const std::string& car_model_path,
                          const std::string& bowl_vert_shader,
                          const std::string& bowl_frag_shader,
                          const std::string& car_vert_shader,
                          const std::string& car_frag_shader) {
    
    std::cout << "Initializing renderer..." << std::endl;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    window = glfwCreateWindow(screen_width, screen_height, 
                             "Surround View - Simple", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(window);
    
    // Load OpenGL functions (using GLFW)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, screen_width, screen_height);
    
    std::cout << "OpenGL initialized" << std::endl;
    
    // Setup bowl
    setupBowl();
    
    // Load bowl shaders
    if (!bowl_shader.loadFromFile(bowl_vert_shader, bowl_frag_shader)) {
        std::cerr << "Failed to load bowl shaders" << std::endl;
        return false;
    }
    
    std::cout << "Bowl shaders loaded" << std::endl;
    
    // Setup car model
    setupCarModel(car_model_path, car_vert_shader, car_frag_shader);
    
    // Create texture
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Create PBO for fast texture upload
    glGenBuffers(1, &pbo_id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 
                 OUTPUT_WIDTH * OUTPUT_HEIGHT * 3, 
                 nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    
    std::cout << "Texture and PBO created" << std::endl;
    
    is_init = true;
    return true;
}

void SVRenderSimple::setupBowl() {
    // Bowl configuration
    bowl_config.disk_radius = 0.4f;
    bowl_config.parab_radius = 0.55f;
    bowl_config.hole_radius = 0.08f;
    bowl_config.a = 0.4f;
    bowl_config.b = 0.4f;
    bowl_config.c = 0.2f;
    bowl_config.vertices_num = 750;
    bowl_config.y_start = 1.0f;
    bowl_config.transformation = glm::mat4(1.0f);
    
    // Generate bowl geometry
    bowl_geometry.createParaboloid(bowl_config);
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    bowl_geometry.getVertices(vertices);
    bowl_geometry.getIndices(indices);
    
    // Create VAO, VBO, EBO
    glGenVertexArrays(1, &bowl_VAO);
    glGenBuffers(1, &bowl_VBO);
    glGenBuffers(1, &bowl_EBO);
    
    glBindVertexArray(bowl_VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, bowl_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bowl_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                         (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    std::cout << "Bowl geometry created: " << indices.size() << " indices" << std::endl;
}

void SVRenderSimple::setupCarModel(const std::string& model_path,
                                   const std::string& vert_shader,
                                   const std::string& frag_shader) {
    // Load car model
    car_model = std::make_unique<Model>(model_path);
    
    // Load car shader
    car_shader = std::make_unique<OGLShader>();
    if (!car_shader->loadFromFile(vert_shader, frag_shader)) {
        std::cerr << "Warning: Failed to load car shaders" << std::endl;
        car_model.reset();
        return;
    }
    
    // Setup car transform
    car_transform = glm::mat4(1.0f);
    car_transform = glm::translate(car_transform, glm::vec3(0.f, 1.01f, 0.f));
    car_transform = glm::rotate(car_transform, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
    car_transform = glm::rotate(car_transform, glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f));
    car_transform = glm::scale(car_transform, glm::vec3(0.002f));
    
    std::cout << "Car model loaded" << std::endl;
}

void SVRenderSimple::textureUpload(const cv::cuda::GpuMat& frame) {
    if (frame.empty()) return;
    
    // Download from GPU to PBO
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    
    if (ptr) {
        frame.download(cv::Mat(OUTPUT_HEIGHT, OUTPUT_WIDTH, CV_8UC3, ptr));
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    
    // Upload to texture
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, OUTPUT_WIDTH, OUTPUT_HEIGHT,
                 0, GL_BGR, GL_UNSIGNED_BYTE, 0);
    
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

bool SVRenderSimple::render(const cv::cuda::GpuMat& stitched_texture) {
    if (!is_init) return false;
    
    // Upload texture
    textureUpload(stitched_texture);
    
    // Clear
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Setup matrices
    glm::mat4 view = camera.getView();
    glm::mat4 projection = glm::perspective(glm::radians(camera.zoom), 
                                           aspect_ratio, 0.1f, 100.f);
    
    // Draw bowl with texture
    glm::mat4 bowl_model = bowl_config.transformation;
    bowl_model = glm::scale(bowl_model, glm::vec3(5.f, 5.f, 5.f));
    
    bowl_shader.useProgramm();
    bowl_shader.setMat4("model", bowl_model);
    bowl_shader.setMat4("view", view);
    bowl_shader.setMat4("projection", projection);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    bowl_shader.setInt("texture1", 0);
    
    glBindVertexArray(bowl_VAO);
    glDrawElements(GL_TRIANGLE_STRIP, bowl_geometry.getIndexCount(), 
                   GL_UNSIGNED_INT, 0);
    
    // Draw car model if loaded
    if (car_model && car_shader) {
        car_shader->useProgramm();
        car_shader->setMat4("model", car_transform);
        car_shader->setMat4("view", view);
        car_shader->setMat4("projection", projection);
        car_model->Draw(*car_shader);
    }
    
    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
    
    return true;
}

bool SVRenderSimple::shouldClose() const {
    return window && glfwWindowShouldClose(window);
}
