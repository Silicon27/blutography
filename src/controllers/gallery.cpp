//
// Created by David Yang on 2026-01-11.
//
#include <controllers/gallery.hpp>

namespace blutography {
    void GalleryController::get(const drogon::HttpRequestPtr& req, Callback_t callback) {
        auto resp = drogon::HttpResponse::newHttpResponse();

        // check on-server cache for image before querying database
        callback(resp);
    }

    void GalleryController::get_image(const drogon::HttpRequestPtr& req, Callback_t callback, std::string&& img_id) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        
        // placeholder for getting image by id
        callback(resp);
    }
}