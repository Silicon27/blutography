#include <support/image_utils.hpp>
#include <turbojpeg.h>
#include <drogon/drogon.h>
#include <vector>
#include <cstring>

namespace blutography::image {

// Helper to extract APP segments from JPEG
static std::vector<std::string> extractAppSegments(const std::string& data) {
    std::vector<std::string> segments;
    if (data.size() < 4) return segments;
    if ((unsigned char)data[0] != 0xFF || (unsigned char)data[1] != 0xD8) return segments;

    size_t pos = 2;
    while (pos + 4 <= data.size()) {
        if ((unsigned char)data[pos] != 0xFF) break;
        unsigned char marker = (unsigned char)data[pos+1];
        if (marker == 0xD9) break; // EOI
        if (marker == 0xDA) break; // SOS - image data follows

        unsigned int length = ((unsigned char)data[pos+2] << 8) | (unsigned char)data[pos+3];
        if (pos + 2 + length > data.size()) break;

        // Collect APP segments (0xE0 - 0xEF)
        if (marker >= 0xE0 && marker <= 0xEF) {
            segments.push_back(data.substr(pos, 2 + length));
        }

        pos += 2 + length;
    }
    return segments;
}

// Helper to insert APP segments into JPEG
static std::string insertAppSegments(const std::string& jpegData, const std::vector<std::string>& segments) {
    if (jpegData.size() < 2 || segments.empty()) return jpegData;

    std::string result;
    result.reserve(jpegData.size() + 2048);
    result.append(jpegData.substr(0, 2)); // SOI (FF D8)

    // Append our segments
    for (const auto& seg : segments) {
        // Skip APP0 (JFIF) from original if we want to use the new one?
        // Actually, just append all, many JPEGs have multiple.
        result.append(seg);
    }

    // Append the rest of the new JPEG, skipping its own SOI
    result.append(jpegData.substr(2));
    
    return result;
}

static uint16_t read16(const unsigned char* data, bool isLittleEndian) {
    if (isLittleEndian) return data[0] | (data[1] << 8);
    return (data[0] << 8) | data[1];
}

static uint32_t read32(const unsigned char* data, bool isLittleEndian) {
    if (isLittleEndian) return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

Metadata extractMetadata(const std::string& inputData) {
    Metadata meta;
    meta.dateTime = "[null]";
    meta.model = "[null]";
    meta.exposure = "[null]";
    meta.iso = "[null]";
    meta.width = 0;
    meta.height = 0;

    tjhandle decompressor = tjInitDecompress();
    if (decompressor) {
        int width, height, subsamp, colorspace;
        if (tjDecompressHeader3(decompressor, (const unsigned char*)inputData.data(), inputData.size(), &width, &height, &subsamp, &colorspace) >= 0) {
            meta.width = width;
            meta.height = height;
        } else {
            LOG_ERROR << "TurboJPEG DecompressHeader failed: " << tjGetErrorStr2(decompressor);
        }
        tjDestroy(decompressor);
    }

    auto segments = extractAppSegments(inputData);
    for (const auto& seg : segments) {
        if ((unsigned char)seg[1] == 0xE1 && seg.size() > 10) { // APP1
            if (std::memcmp(&seg[4], "Exif\0\0", 6) == 0) {
                const unsigned char* tiffBase = (const unsigned char*)&seg[10];
                size_t tiffSize = seg.size() - 10;
                
                bool isLittleEndian = (tiffBase[0] == 'I');
                uint32_t firstIfdOffset = read32(tiffBase + 4, isLittleEndian);
                
                auto parseIfd = [&](uint32_t offset, auto& self) -> void {
                    if (offset + 2 > tiffSize) return;
                    uint16_t numEntries = read16(tiffBase + offset, isLittleEndian);
                    for (int i = 0; i < numEntries; ++i) {
                        uint32_t entryOffset = offset + 2 + i * 12;
                        if (entryOffset + 12 > tiffSize) break;
                        
                        uint16_t tag = read16(tiffBase + entryOffset, isLittleEndian);
                        uint16_t type = read16(tiffBase + entryOffset + 2, isLittleEndian);
                        uint32_t count = read32(tiffBase + entryOffset + 4, isLittleEndian);
                        uint32_t valOffset = read32(tiffBase + entryOffset + 8, isLittleEndian);
                        
                        if (tag == 0x0110) { // Model
                            if (type == 2 && valOffset + count <= tiffSize) {
                                meta.model = std::string((const char*)tiffBase + valOffset, count);
                                if (!meta.model.empty() && meta.model.back() == '\0') meta.model.pop_back();
                            }
                        } else if (tag == 0x8769) { // Exif Offset
                            self(valOffset, self);
                        } else if (tag == 0x9003) { // DateTimeOriginal
                            if (type == 2 && valOffset + count <= tiffSize) {
                                meta.dateTime = std::string((const char*)tiffBase + valOffset, count);
                                if (!meta.dateTime.empty() && meta.dateTime.back() == '\0') meta.dateTime.pop_back();
                            }
                        } else if (tag == 0x829a) { // ExposureTime
                            if (type == 5 && valOffset + 8 <= tiffSize) {
                                uint32_t num = read32(tiffBase + valOffset, isLittleEndian);
                                uint32_t den = read32(tiffBase + valOffset + 4, isLittleEndian);
                                if (num == 1 && den > 1) meta.exposure = "1/" + std::to_string(den);
                                else if (den > 0) meta.exposure = std::to_string((float)num/den) + "s";
                            }
                        } else if (tag == 0x8827) { // ISO
                            if (type == 3) {
                                meta.iso = std::to_string(read16(tiffBase + entryOffset + 8, isLittleEndian));
                            }
                        }
                    }
                };
                parseIfd(firstIfdOffset, parseIfd);
            }
        }
    }

    return meta;
}

std::string createGalleryPreview(const std::string& inputData, const std::string& fileName) {
    if (inputData.size() < 4) return inputData;
    
    // Basic JPEG detection
    bool isJpeg = ((unsigned char)inputData[0] == 0xFF && (unsigned char)inputData[1] == 0xD8);
    
    if (!isJpeg) {
        LOG_DEBUG << "File " << fileName << " is not a JPEG, skipping compression.";
        return inputData; 
    }

    // 1. Extract APP segments (EXIF etc) from original
    auto appSegments = extractAppSegments(inputData);

    tjhandle decompressor = tjInitDecompress();
    if (!decompressor) return inputData;

    int width, height, subsamp, colorspace;
    if (tjDecompressHeader3(decompressor, (const unsigned char*)inputData.data(), inputData.size(), &width, &height, &subsamp, &colorspace) < 0) {
        tjDestroy(decompressor);
        return inputData;
    }

    tjscalingfactor scalingFactor = {1, 1};
    if (width > 8000 || height > 8000) {
        scalingFactor = {1, 2};
    }

    int scaledWidth = TJSCALED(width, scalingFactor);
    int scaledHeight = TJSCALED(height, scalingFactor);

    std::vector<unsigned char> rawBuffer(scaledWidth * scaledHeight * 3); // RGB
    if (tjDecompress2(decompressor, (const unsigned char*)inputData.data(), inputData.size(), rawBuffer.data(), scaledWidth, 0, scaledHeight, TJPF_RGB, TJFLAG_FASTDCT) < 0) {
        tjDestroy(decompressor);
        return inputData;
    }
    tjDestroy(decompressor);

    tjhandle compressor = tjInitCompress();
    if (!compressor) return inputData;

    unsigned char* compressedData = nullptr;
    unsigned long compressedSize = 0;
    int quality = 90; 
    
    if (tjCompress2(compressor, rawBuffer.data(), scaledWidth, 0, scaledHeight, TJPF_RGB, &compressedData, &compressedSize, subsamp, quality, TJFLAG_FASTDCT) < 0) {
        if (compressedData) tjFree(compressedData);
        tjDestroy(compressor);
        return inputData;
    }

    std::string compressedStr((char*)compressedData, compressedSize);
    tjFree(compressedData);
    tjDestroy(compressor);

    // 2. Insert original APP segments back into the compressed JPEG
    std::string result = insertAppSegments(compressedStr, appSegments);

    LOG_INFO << "Generated preview for " << fileName << " (" << width << "x" << height << "): " 
             << inputData.size() / 1024 << "KB -> " << result.size() / 1024 << "KB (EXIF preserved)";
    
    return result;
}

}
