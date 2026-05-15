#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <glad/glad.h>

// NOTE: glad must be included before any OpenGL or GLFW headers in this translation unit.

#include "Image.h"
#include <stb_image.h>

// (rest of file unchanged)
