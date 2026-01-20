#ifndef BLUTOGRAPHY_GALLERY_STORAGE_HPP
#define BLUTOGRAPHY_GALLERY_STORAGE_HPP

#include <string>
#include <vector>
#include <json/json.h>
#include <mutex>
#include <support/image_utils.hpp>

namespace blutography {

struct GalleryItem {
    std::string id;
    std::string name;
    std::string quote;
    std::string fileName;
    std::string previewName;
    image::Metadata metadata;
};

class GalleryStorage {
public:
    static GalleryStorage& instance();

    void addItem(const GalleryItem& item);
    std::vector<GalleryItem> getAllItems();
    std::optional<GalleryItem> getItem(const std::string& id);

private:
    GalleryStorage();
    void load();
    void save();

    std::vector<GalleryItem> items_;
    std::mutex mutex_;
    std::string storagePath_ = "gallery_data.json";
};

}

#endif // BLUTOGRAPHY_GALLERY_STORAGE_HPP
