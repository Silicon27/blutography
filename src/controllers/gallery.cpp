//
// Created by David Yang on 2026-01-11.
//
#include <controllers/gallery.hpp>

namespace blutography {
    void GalleryController::get(const drogon::HttpRequestPtr& req, Callback_t callback) {
        // Offload to background loop to keep IO threads free for more users
        drogon::app().getLoop()->queueInLoop([callback]() {
            auto resp = drogon::HttpResponse::newHttpResponse();
            // TODO: Get preview on-server and serve to client




            // TODO: Implement a custom upload handler for images uploaded by me, the admin
            // this is to void the difficulties of creating a lower resolution image and storing as preview manually
            // on-server and then uploading the high fidelity version to the database
            // NOTE: would require authentication
            callback(resp);
        });
    }

    void GalleryController::get_image(const drogon::HttpRequestPtr& req, Callback_t callback, std::string&& img_id) {
        // Offload to background loop to keep IO threads free for more users
        drogon::app().getLoop()->queueInLoop([callback, id = std::move(img_id)]() {
            auto resp = drogon::HttpResponse::newHttpResponse();
            // TODO: Implement image retrieval by id


            callback(resp);
        });
    }
}