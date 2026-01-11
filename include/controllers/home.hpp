//
// Created by David Yang on 2026-01-08.
//

#ifndef BLUTOGRAPHY_HOME_HPP
#define BLUTOGRAPHY_HOME_HPP
#include <drogon/HttpController.h>

#include <support/controllers.hpp>

namespace blutography {
    class Home_Controller : public drogon::HttpController<Home_Controller> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(Home_Controller::index, "/", drogon::Get);
        METHOD_LIST_END

        void index(const drogon::HttpRequestPtr &req,
                    Callback_t callback);
    };
}

#endif //BLUTOGRAPHY_HOME_HPP