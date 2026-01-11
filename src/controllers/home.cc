//
// Created by David Yang on 2026-01-08.
//

#include <controllers/home.hpp>

namespace blutography {
    void Home_Controller::index(const drogon::HttpRequestPtr &req, Callback_t callback) {
        const auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/home.html");
        callback(resp);
    }
}
