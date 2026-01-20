//
// Created by David Yang on 2026-01-11.
//

#ifndef BLUTOGRAPHY_GALLERY_HPP
#define BLUTOGRAPHY_GALLERY_HPP
#include <drogon/HttpController.h>

#include <support/controllers.hpp>

namespace blutography {
    class GalleryController : public drogon::HttpController<GalleryController>
    {
    public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GalleryController::get, "/gallery", drogon::Get);
    ADD_METHOD_TO(GalleryController::get_data, "/gallery/data", drogon::Get);
    ADD_METHOD_TO(GalleryController::get_previews_bundle, "/gallery/previews", drogon::Get);
    ADD_METHOD_TO(GalleryController::get_preview_image, "/gallery/preview/{1}", drogon::Get);
    ADD_METHOD_TO(GalleryController::get_image, "/gallery/image/{1}", drogon::Get);
    ADD_METHOD_TO(GalleryController::download_bundle, "/gallery/download", drogon::Post);
    METHOD_LIST_END

    void get(const drogon::HttpRequestPtr& req, Callback_t callback);
    void get_data(const drogon::HttpRequestPtr& req, Callback_t callback);
    void get_previews_bundle(const drogon::HttpRequestPtr& req, Callback_t callback);
    void get_preview_image(const drogon::HttpRequestPtr& req, Callback_t callback, const std::string& filename);
    void get_image(const drogon::HttpRequestPtr& req, Callback_t callback, const std::string& imageId);
    void download_bundle(const drogon::HttpRequestPtr& req, Callback_t callback);
    };
}


#endif //BLUTOGRAPHY_GALLERY_HPP