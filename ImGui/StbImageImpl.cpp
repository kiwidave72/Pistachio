// Single translation unit for stb_image implementation.
//
// This file should be compiled into the imgui shared library (imgui.dll) so that
// the exported stb_* symbols listed in exports.def resolve correctly.
//
// IMPORTANT: Do not define STB_IMAGE_IMPLEMENTATION anywhere else.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
