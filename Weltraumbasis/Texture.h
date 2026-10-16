#pragma once
#include <string>
#include <cstddef>    
#include <GL/glew.h>  

GLuint LoadTexture2D_RGBA(const std::string& path);
GLuint LoadTexture2D_FromMemory(const unsigned char* data, size_t sizeBytes);
