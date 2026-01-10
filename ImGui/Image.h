#pragma once

#include <string>
#include <cstdint>

namespace Walnut {

	enum class ImageFormat
	{
		None = 0,
		RGBA,
		RGBA32F
	};

	class Image
	{
	public:
		Image(std::string_view path);
		Image(uint32_t width, uint32_t height, ImageFormat format, const void* data = nullptr);
		~Image();

		void SetData(const void* data);
		void Resize(uint32_t width, uint32_t height);

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// For ImGui::Image() - returns the OpenGL texture ID cast to void*
		void* GetDescriptorSet() const { return (void*)(intptr_t)m_TextureID; }
		uint32_t GetTextureID() const { return m_TextureID; }

		static void* Decode(const void* buffer, uint64_t length, uint32_t& outWidth, uint32_t& outHeight);

	private:
		void AllocateMemory(uint64_t size);
		void Release();

	private:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_TextureID = 0;
		ImageFormat m_Format = ImageFormat::RGBA;
		std::string m_Filepath;
	};

}