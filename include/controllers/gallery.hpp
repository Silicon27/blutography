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
        METHOD_ADD(GalleryController::get, "/gallery", drogon::Get);
        METHOD_ADD(GalleryController::get_image, "/gallery?img_id={1}", drogon::Get);
        METHOD_LIST_END

        void get(const drogon::HttpRequestPtr& req, Callback_t callback);
        void get_image(const drogon::HttpRequestPtr& req, Callback_t callback, std::string&& img_id);

    };
}


#endif //BLUTOGRAPHY_GALLERY_HPP