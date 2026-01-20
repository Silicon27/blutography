#ifndef BLUTOGRAPHY_ADMIN_HPP
#define BLUTOGRAPHY_ADMIN_HPP

#include <drogon/HttpController.h>

namespace blutography {
    class Admin_Controller : public drogon::HttpController<Admin_Controller> {
    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(Admin_Controller::loginPage, "/login", drogon::Get);
            ADD_METHOD_TO(Admin_Controller::login, "/login", drogon::Post);
            ADD_METHOD_TO(Admin_Controller::uploadPage, "/upload", drogon::Get, "blutography::AdminAuthFilter");
            ADD_METHOD_TO(Admin_Controller::uploadImage, "/upload", drogon::Post, "blutography::AdminAuthFilter");
            ADD_METHOD_TO(Admin_Controller::b2Test, "/b2_test", drogon::Get, "blutography::AdminAuthFilter");
        METHOD_LIST_END

        void loginPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
        void login(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
        void uploadPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
        void uploadImage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
        void b2Test(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    };
}

#endif //BLUTOGRAPHY_ADMIN_HPP
