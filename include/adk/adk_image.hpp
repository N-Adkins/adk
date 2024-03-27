/*  
    ADK_Image - v0.1
    No warranty implied, use at your own risk.
    
    Very high-level api to load images into memory and access them. Intended for simple
    usage where success is expected and without excessive parameters or compression.

    The public interface is adk::image, adk::image::internal should not be accessed
    unless you know what you're doing.
*/

#ifndef ADK_IMAGE_HPP
#define ADK_IMAGE_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <vector>

// User can define AECS_USE_ASSERTIONS to enable debugging assertions.
// Off by default.
#ifdef ADK_USE_ASSERTIONS
    #define ADK_ASSERT(condition) assert(condition)
#else
    #define ADK_ASSERT(...) ((void)0);
#endif

namespace adk::image::internal
{

enum class format : std::uint8_t
{
    png,
};

namespace png
{

inline std::uint32_t u32_big_endian(const uint8_t* bytes, std::size_t index)
{
    const std::uint32_t value = bytes[index] | (bytes[index+1] << 8) 
        | (bytes[index+2] << 16) | (bytes[index+3] << 24);
    return value;
}

constexpr struct 
{
    std::array<std::uint8_t, 8> signature = { 137, 80, 78, 71, 13, 10, 26, 10 };
    std::uint32_t ihdr_name = 0x49484452;
    std::uint32_t plte_name = 0x504C5445;
    std::uint32_t iend_name = 0x49454E44;
} info;

struct chunk 
{
    std::uint32_t name;
    std::span<std::uint8_t> data; 
    std::uint32_t crc;
};

struct palette_color
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

struct file
{
    std::vector<std::uint8_t> raw;
    
    std::optional<std::vector<palette_color>> palette = std::nullopt;

    std::size_t index = 0;
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t bit_depth;
    std::uint8_t color_type;
    std::uint8_t compression_method;
    std::uint8_t filter_method;
    std::uint8_t interlace_method;

    inline bool check_signature()
    {
        if (std::memcmp(info.signature.data(), raw.data(), info.signature.size()) == 0) {
            index += info.signature.size();
            return true;
        }
        return false;
    }

    inline std::uint32_t next_u32() 
    {
        const std::uint32_t value = u32_big_endian(raw.data(), index);
        index += 4;
        return value;
    }

    inline std::uint8_t next_u8()
    {
        return raw[index++];
    }

    inline chunk next_chunk()
    {
        const auto size = next_u32();
        const auto raw_name = next_u32();
        std::span<std::uint8_t> data = std::span(&raw[index], size);
        index += size;
        const auto crc = next_u32();
        return chunk{ 
            .name = raw_name,
            .data = data,
            .crc = crc,
        };
    }

    inline bool process_plte(const chunk& chunk)
    {
        ADK_ASSERT(chunk.name == info.plte_name);
        
        // Chunk size must be divisible by 3
        if (chunk.data.size() % 3 != 0) {
            return false;
        }

        std::vector<palette_color> new_palette;

        for (int i = 0; i < chunk.data.size(); i += 3) {
            new_palette.push_back(palette_color{
                .r = chunk.data[i],
                .g = chunk.data[i+1],
                .b = chunk.data[i+2],
            });
        }

        palette = new_palette;

        return true;
    }

    inline bool process_ihdr()
    {
        const auto ihdr_chunk = next_chunk();
        if (ihdr_chunk.name != info.ihdr_name) {
            return false;
        }

        if (ihdr_chunk.data.size() < 12) {
            return false;
        }
        
        // 0 is invalid for width and height
        width = u32_big_endian(ihdr_chunk.data.data(), 0);
        height = u32_big_endian(ihdr_chunk.data.data(), 4);
        if (width == 0 || height == 0) {
            return false;
        }

        bit_depth = ihdr_chunk.data[8];
        color_type = ihdr_chunk.data[9];
        
        // Only 0 is a defined compression method
        compression_method = ihdr_chunk.data[10];
        if (compression_method == 0) {
            return false;
        }

        filter_method = ihdr_chunk.data[11];
        interlace_method = ihdr_chunk.data[12];

        return true;
    }
};

} // namespace png

/**
 * Base template for all file decoding, must be specialized for each format.
 */
template <format file_format>
std::optional<std::vector<std::uint8_t>> decode(
        const std::filesystem::path& path, std::uint8_t channels);

// PNG Decoding
template <>
inline std::optional<std::vector<std::uint8_t>> decode<format::png>(
        const std::filesystem::path& path, const std::uint8_t channels)
{
    png::file file;

    const std::size_t file_size = std::filesystem::file_size(path);
    std::ifstream in_file(path, std::ios::binary);

    file.raw.resize(file_size);
    in_file.read(reinterpret_cast<char*>(file.raw.data()), file_size);
    
    // File signature must be correct for PNG
    if (!file.check_signature()) {
        return std::nullopt;
    }

    if (!file.process_ihdr()) {
        return std::nullopt;
    }
    
    bool reached_iend = false;
    while (file.index < file.raw.size() && !reached_iend) {
        const auto chunk = file.next_chunk();
        switch (chunk.name)
        {
        case png::info.plte_name:
            file.process_plte(chunk);
            break;
        case png::info.iend_name:
            reached_iend = true;
            break;
        default:
            break;
        }
    }

    return file.raw; // change this
}

} // namespace adk::image::internal

namespace adk::image
{

/**
 * Specifies a number of color channels, used within
 */
enum class channels : std::uint8_t
{
    monochrome = 1,
    rgb = 3,
    rgba = 4,
};

/**
 * Image data wrapper, cannot be copied as it holds ownership of the image memory.
 */
class image
{
public:
    /**
     * Returns the raw bytes of the image, mainly for use with C apis like OpenGL or similar.
     */
    inline const std::uint8_t* get_raw() const
    {
        return bytes.data();
    }

private:
    channels channel_count = channels::rgba;
    std::vector<std::uint8_t> bytes;

    friend std::optional<image> from_path(const std::filesystem::path& path, channels channels);
};

inline std::optional<image> from_path(const std::filesystem::path& path, channels channels)
{
    image new_image{};

    const auto extension = path.extension();
    if (extension == ".png") {
        const auto maybe_bytes = internal::decode<internal::format::png>(path, static_cast<std::uint8_t>(channels));
        if (!maybe_bytes) {
            return std::nullopt;
        } else {
            new_image.bytes = maybe_bytes.value();
        }
    } else {
        ADK_ASSERT(false);
    }

    return new_image;
}

} // namespace adk::image

#undef ADK_ASSERT

#endif
