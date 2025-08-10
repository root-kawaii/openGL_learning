#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <iostream>
#include <string>

class Texture {
public:
    enum class Type {
        TEXTURE_2D,
        TEXTURE_2D_MULTISAMPLE,
        TEXTURE_CUBE_MAP,
        DEPTH_2D,
        DEPTH_CUBE_MAP
    };

    enum class Format {
        RGBA,
        RGBA16F,
        RGBA8,
        RGB,
        R32F,
        DEPTH_COMPONENT,
        DEPTH_COMPONENT24
    };

    enum class Filter {
        NEAREST,
        LINEAR
    };

    enum class Wrap {
        REPEAT,
        CLAMP_TO_EDGE,
        MIRRORED_REPEAT
    };

private:


public:
    // Constructors
    Texture();
    Texture(char const * path);
    ~Texture();

    // Delete copy constructor and assignment operator to prevent copying
    // Texture(const Texture&) = delete;
    // Texture& operator=(const Texture&) = delete;

    // // Move constructor and assignment operator
    // Texture(Texture&&) = default;
    // Texture& operator=(Texture&&) = default;

    unsigned int id;
    std::string path;
    std::string type;
    Type m_type;
    Format m_format;
    int m_width, m_height;
    int m_samples; // For multisampled textures
    bool m_isLoaded;


    // // Create empty texture
    // bool create2D(int width, int height, Format format, Filter minFilter = Filter::LINEAR, 
    //               Filter magFilter = Filter::LINEAR, Wrap wrapS = Wrap::CLAMP_TO_EDGE, 
    //               Wrap wrapT = Wrap::CLAMP_TO_EDGE);

    // // Create multisampled texture
    // bool create2DMultisample(int width, int height, Format format, int samples = 4);

    // // Create depth texture
    // bool createDepth2D(int width, int height, Filter filter = Filter::LINEAR, 
    //                    Wrap wrapS = Wrap::CLAMP_TO_EDGE, Wrap wrapT = Wrap::CLAMP_TO_EDGE);

    // // Create depth cubemap
    // bool createDepthCubemap(int width, int height);

    // // Load texture from file
    // bool loadFromFile(const std::string& filepath);

    // // Load cubemap from files
    // bool loadCubemap(const std::vector<std::string>& faces);

    // Load cubemap from files
    unsigned int loadTexture(char const * path);    

    // // Bind texture
    // void bind(unsigned int slot = 0) const;
    // void unbind() const;

    // // Getters

    // // Cleanup
    void destroy();

private:
    // GLenum getGLType() const;
    // GLenum getGLFormat() const;
    // GLenum getGLInternalFormat() const;
    // GLenum getGLDataType() const;
    // GLenum getGLFilter(Filter filter) const;
    // GLenum getGLWrap(Wrap wrap) const;
};