// This file ensures stb_image implementation is compiled exactly once.
// It fixes unresolved externals like stbi_image_free.
//
// Keep this in the imgui static library (or any single target).
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
