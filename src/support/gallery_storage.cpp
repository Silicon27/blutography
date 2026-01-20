#include <support/gallery_storage.hpp>
#include <fstream>
#include <drogon/drogon.h>

namespace blutography {

GalleryStorage& GalleryStorage::instance() {
    static GalleryStorage inst;
    return inst;
}

GalleryStorage::GalleryStorage() {
    load();
}

void GalleryStorage::addItem(const GalleryItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.push_back(item);
    save();
}

std::vector<GalleryItem> GalleryStorage::getAllItems() {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_;
}

std::optional<GalleryItem> GalleryStorage::getItem(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : items_) {
        if (item.id == id) return item;
    }
    return std::nullopt;
}

void GalleryStorage::load() {
    std::ifstream ifile(storagePath_);
    if (!ifile.is_open()) return;

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifile, &root, &errs)) {
        LOG_ERROR << "Failed to parse gallery_data.json: " << errs;
        return;
    }

    items_.clear();
    for (const auto& jItem : root) {
        GalleryItem item;
        item.id = jItem["id"].asString();
        item.name = jItem["name"].asString();
        item.quote = jItem["quote"].asString();
        item.fileName = jItem["fileName"].asString();
        item.previewName = jItem["previewName"].asString();
        item.metadata.dateTime = jItem["metadata"]["dateTime"].asString();
        item.metadata.model = jItem["metadata"]["model"].asString();
        item.metadata.exposure = jItem["metadata"].get("exposure", "[null]").asString();
        item.metadata.iso = jItem["metadata"].get("iso", "[null]").asString();
        item.metadata.width = jItem["metadata"]["width"].asInt();
        item.metadata.height = jItem["metadata"]["height"].asInt();
        items_.push_back(item);
    }
}

void GalleryStorage::save() {
    Json::Value root(Json::arrayValue);
    for (const auto& item : items_) {
        Json::Value jItem;
        jItem["id"] = item.id;
        jItem["name"] = item.name;
        jItem["quote"] = item.quote;
        jItem["fileName"] = item.fileName;
        jItem["previewName"] = item.previewName;
        jItem["metadata"]["dateTime"] = item.metadata.dateTime;
        jItem["metadata"]["model"] = item.metadata.model;
        jItem["metadata"]["exposure"] = item.metadata.exposure;
        jItem["metadata"]["iso"] = item.metadata.iso;
        jItem["metadata"]["width"] = item.metadata.width;
        jItem["metadata"]["height"] = item.metadata.height;
        root.append(jItem);
    }

    std::ofstream ofile(storagePath_);
    if (ofile.is_open()) {
        Json::StreamWriterBuilder builder;
        ofile << Json::writeString(builder, root);
    }
}

}
