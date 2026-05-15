#include "Walnut/Image.h"

// OpenGL
#include <glad/glad.h>

// stb
#include <stb_image.h>

#include <cassert>

namespace Walnut
{
	static GLenum ToGLInternalFormat(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::RGBA: return GL_RGBA8;
			default: return GL_RGBA8;
		}
	}

	static GLenum ToGLDataFormat(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::RGBA: return GL_RGBA;
			default: return GL_RGBA;
		}
	}

	Image::Image(uint32_t width, uint32_t height, ImageFormat format, const void* data)
		: m_Width(width), m_Height(height), m_Format(format)
	{
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		const GLenum internalFormat = ToGLInternalFormat(format);
		const GLenum dataFormat = ToGLDataFormat(format);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)width, (GLsizei)height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
	}

	Image::~Image()
	{
		if (m_RendererID)
			glDeleteTextures(1, &m_RendererID);
		m_RendererID = 0;
	}

	void* Image::Decode(const void* data, uint64_t length, uint32_t& outWidth, uint32_t& outHeight)
	{
		int w = 0, h = 0, channels = 0;
		stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*)data, (int)length, &w, &h, &channels, 4);
		if (!pixels)
		{
			outWidth = outHeight = 0;
			return nullptr;
		}
		outWidth = (uint32_t)w;
		outHeight = (uint32_t)h;
		return pixels;
	}
}
