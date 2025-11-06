#ifndef OGL_SHADER_HPP
#define OGL_SHADER_HPP

#include <string>
#include <GLES3/gl3.h>  // OpenGL ES 3.x for Jetson
#include <glm/glm.hpp>

/**
 * @brief OpenGL Shader Wrapper
 * 
 * Handles loading, compiling, and using GLSL shaders.
 * Provides methods to set uniform variables.
 */
class OGLShader {
public:
    // Shader program ID
    unsigned int ID;
    
    // Constructors
    OGLShader();
    OGLShader(const char* vertexPath, const char* fragmentPath);
    
    // Destructor
    ~OGLShader();
    
    /**
     * @brief Load shader from files
     * @param vertexPath Path to vertex shader file
     * @param fragmentPath Path to fragment shader file
     * @return true if successful
     */
    bool loadFromFile(const std::string& vertexPath, const std::string& fragmentPath);
    
    /**
     * @brief Activate the shader
     */
    void useProgramm() const;
    void use() const;
    
    // Utility uniform functions
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec2(const std::string &name, const glm::vec2 &value) const;
    void setVec2(const std::string &name, float x, float y) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setVec4(const std::string &name, float x, float y, float z, float w) const;
    void setMat2(const std::string &name, const glm::mat2 &mat) const;
    void setMat3(const std::string &name, const glm::mat3 &mat) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
    
private:
    /**
     * @brief Check for shader compilation/linking errors
     * @param shader Shader ID
     * @param type Shader type ("VERTEX", "FRAGMENT", "PROGRAM")
     * @param path File path for error reporting
     * @return true if no errors
     */
    bool checkCompileErrors(unsigned int shader, const std::string& type, const std::string& path);
};

#endif // OGL_SHADER_HPP