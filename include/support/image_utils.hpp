#ifndef BLUTOGRAPHY_IMAGE_UTILS_HPP
#define BLUTOGRAPHY_IMAGE_UTILS_HPP

#include <string>
#include <vector>

namespace blutography::image {
    struct Metadata {
        std::string dateTime;
        std::string model;
        std::string exposure;
        std::string iso;
        int width = 0;
        int height = 0;
    };

    /**
     * @brief Compresses an image to JPEG format aiming for a 3-5 MB size, preserving EXIF.
     * @param inputData The raw bytes of the original image (JPEG, PNG, etc).
     * @param fileName The name of the file (used to detect format).
     * @return The compressed JPEG data as a string.
     */
    std::string createGalleryPreview(const std::string& inputData, const std::string& fileName);

    /**
     * @brief Extracts EXIF metadata from a JPEG image.
     * @param inputData The raw bytes of the JPEG image.
     * @return Metadata struct with extracted fields.
     */
    Metadata extractMetadata(const std::string& inputData);
}

#endif // BLUTOGRAPHY_IMAGE_UTILS_HPP
