#pragma once

#include <cstdint>

namespace Walnut
{
	enum class ImageFormat
	{
		None = 0,
		RGBA
	};

	// Minimal Walnut::Image replacement used by Pistachio.
	// Provides texture upload + stb decode.
	class Image
	{
	public:
		Image(uint32_t width, uint32_t height, ImageFormat format, const void* data);
		~Image();

		uint32_t GetRendererID() const { return m_RendererID; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		// Decodes image bytes (PNG/JPG/etc) into RGBA8 pixels.
		// Returned pointer must be freed with stbi_image_free.
		static void* Decode(const void* data, uint64_t length, uint32_t& outWidth, uint32_t& outHeight);

	private:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		ImageFormat m_Format = ImageFormat::None;
		uint32_t m_RendererID = 0;
	};
}
