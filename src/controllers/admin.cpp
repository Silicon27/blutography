#include <controllers/admin.hpp>
#include <filters/adminfilter.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/MultiPart.h>

namespace blutography {
    void Admin_Controller::loginPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/login.html");
        callback(resp);
    }

    void Admin_Controller::login(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        const auto password = req->getParameter("password");

        std::string admin_pass = std::getenv("ADMIN_PASSWORD");

        if (password == admin_pass) {
            auto session = req->getSession();
            session->insert("is_admin", true);
            
            auto resp = drogon::HttpResponse::newRedirectionResponse("/upload");
            callback(resp);
            return;
        }
        
        // Failed login, redirect back to login page with error
        auto resp = drogon::HttpResponse::newRedirectionResponse("/login?error=1");
        callback(resp);
    }

    void Admin_Controller::uploadPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/upload.html");
        callback(resp);
    }

    void Admin_Controller::uploadImage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        drogon::MultiPartParser fileUpload;
        if (fileUpload.parse(req) != 0 || fileUpload.getFiles().empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setBody("Invalid upload request");
            callback(resp);
            return;
        }

        auto &file = fileUpload.getFiles()[0];
        // The file is automatically saved to the upload_path defined in config
        file.save(); 

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody("<html><body><h2>File uploaded successfully</h2><a href='/upload'>Back to upload</a></body></html>");
        callback(resp);
    }
}
