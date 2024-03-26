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

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

// User can define AECS_USE_ASSERTIONS to enable debugging assertions.
// Off by default.
#ifdef ADK_USE_ASSERTIONS
    #define ADK_ASSERT(condition) assert(condition)
#else
    #define ADK_ASSERT(...) ((void)0);
#endif

#undef ADK_ASSERT

namespace adk::image::internal
{

enum class format : std::uint8_t
{
    jpeg,
};

/**
 * Base template for all file decoding, must be specialized for each format.
 */
template <format file_format>
std::optional<std::vector<std::uint8_t>> decode(const std::filesystem::path& path, std::uint8_t channels);

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
};

} // namespace adk::image

#endif
