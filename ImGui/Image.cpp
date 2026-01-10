#include "Image.h"

// Include OpenGL headers before GLFW
#ifdef _WIN32
#include <windows.h>
#endif

#include <GLFW/glfw3.h>

// Define GL_RGBA32F if not available (OpenGL 3.0+)
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Walnut {

	namespace Utils {

		static uint32_t BytesPerPixel(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return 4;
			case ImageFormat::RGBA32F: return 16;
			}
			return 0;
		}

		static GLenum WalnutFormatToGLFormat(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return GL_RGBA;
			case ImageFormat::RGBA32F: return GL_RGBA;
			}
			return GL_RGBA;
		}

		static GLenum WalnutFormatToGLInternalFormat(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return GL_RGBA8;
			case ImageFormat::RGBA32F: return GL_RGBA32F;
			}
			return GL_RGBA8;
		}

		static GLenum WalnutFormatToGLType(ImageFormat format)
		{
			switch (format)
			{
			case ImageFormat::RGBA:    return GL_UNSIGNED_BYTE;
			case ImageFormat::RGBA32F: return GL_FLOAT;
			}
			return GL_UNSIGNED_BYTE;
		}

	}

	Image::Image(std::string_view path)
		: m_Filepath(path)
	{
		int width, height, channels;
		uint8_t* data = nullptr;

		if (stbi_is_hdr(m_Filepath.c_str()))
		{
			data = (uint8_t*)stbi_loadf(m_Filepath.c_str(), &width, &height, &channels, 4);
			m_Format = ImageFormat::RGBA32F;
		}
		else
		{
			data = stbi_load(m_Filepath.c_str(), &width, &height, &channels, 4);
			m_Format = ImageFormat::RGBA;
		}

		if (!data)
		{
			// Handle error - could throw or set to default 1x1 texture
			m_Width = 1;
			m_Height = 1;
			AllocateMemory(4);
			uint32_t magenta = 0xFFFF00FF;
			SetData(&magenta);
			return;
		}

		m_Width = width;
		m_Height = height;

		AllocateMemory(m_Width * m_Height * Utils::BytesPerPixel(m_Format));
		SetData(data);
		stbi_image_free(data);
	}

	Image::Image(uint32_t width, uint32_t height, ImageFormat format, const void* data)
		: m_Width(width), m_Height(height), m_Format(format)
	{
		AllocateMemory(m_Width * m_Height * Utils::BytesPerPixel(m_Format));
		if (data)
			SetData(data);
	}

	Image::~Image()
	{
		Release();
	}

	void Image::AllocateMemory(uint64_t size)
	{
		// Create OpenGL texture
		glGenTextures(1, &m_TextureID);
		glBindTexture(GL_TEXTURE_2D, m_TextureID);

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// Allocate texture storage
		GLenum internalFormat = Utils::WalnutFormatToGLInternalFormat(m_Format);
		GLenum format = Utils::WalnutFormatToGLFormat(m_Format);
		GLenum type = Utils::WalnutFormatToGLType(m_Format);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, format, type, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Image::Release()
	{
		if (m_TextureID)
		{
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
	}

	void Image::SetData(const void* data)
	{
		glBindTexture(GL_TEXTURE_2D, m_TextureID);

		GLenum format = Utils::WalnutFormatToGLFormat(m_Format);
		GLenum type = Utils::WalnutFormatToGLType(m_Format);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, type, data);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void Image::Resize(uint32_t width, uint32_t height)
	{
		if (m_TextureID && m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		Release();
		AllocateMemory(m_Width * m_Height * Utils::BytesPerPixel(m_Format));
	}

	void* Image::Decode(const void* buffer, uint64_t length, uint32_t& outWidth, uint32_t& outHeight)
	{
		int width, height, channels;
		uint8_t* data = nullptr;

		data = stbi_load_from_memory((const stbi_uc*)buffer, length, &width, &height, &channels, 4);

		outWidth = width;
		outHeight = height;

		return data;
	}
}