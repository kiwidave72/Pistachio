#pragma once

// Legacy header kept for compatibility with older build scripts.
//
// Pistachio now uses glad for OpenGL function loading. Include this header
// if some target still references GLLoader.h, but avoid redeclaring OpenGL
// entry points (which breaks when <GL/gl.h> is pulled in by vcpkg).

#include "adapters/rendering/OpenGLApi.h"

namespace adapters {
    // No-op. All OpenGL symbols come from glad.
}
