#include <controllers/admin.hpp>
#include <filters/adminfilter.hpp>
#include <support/b2service.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/MultiPart.h>
#include <atomic>
#include <mutex>
#include <algorithm>

namespace blutography {
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    void Admin_Controller::loginPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/login.html");
        callback(resp);
    }

    void Admin_Controller::login(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        std::string raw_password = req->getParameter("password");
        std::string password = trim(raw_password);

        const char* env_admin_pass = std::getenv("ADMIN_PASSWORD");
        std::string admin_pass = env_admin_pass ? trim(env_admin_pass) : "";

        if (!admin_pass.empty() && !password.empty() && password == admin_pass) {
            auto session = req->getSession();
            if (session) {
                session->insert("is_admin", true);
                
                auto resp = drogon::HttpResponse::newRedirectionResponse("/upload");
                callback(resp);
                return;
            } else {
                LOG_ERROR << "Session is not enabled, cannot login";
            }
        } else {
            if (admin_pass.empty()) {
                LOG_WARN << "ADMIN_PASSWORD environment variable is not set or empty";
            } else if (password.empty()) {
                LOG_DEBUG << "Login failed: empty password received (parameter present: " 
                          << (req->getParameters().count("password") > 0 ? "yes" : "no") << ")";
            } else if (password != admin_pass) {
                LOG_DEBUG << "Login failed: password mismatch (input length: " << password.length() 
                          << ", expected length: " << admin_pass.length() << ")";
            }
        }
        
        // Failed login, redirect back to login page with error
        auto resp = drogon::HttpResponse::newRedirectionResponse("/login?error=1");
        callback(resp);
    }

    void Admin_Controller::uploadPage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/upload.html");
        callback(resp);
    }

    static std::shared_ptr<B2Service> getB2Service() {
        auto customConfig = drogon::app().getCustomConfig();
        const auto& b2Config = customConfig["b2"];
        
        std::string keyId = b2Config.get("keyId", "").asString();
        std::string bucketName = b2Config.get("bucketName", "").asString();
        const char* env_api_key = std::getenv("DB_API_KEY");
        std::string applicationKey = env_api_key ? env_api_key : "";

        if (keyId.empty() || bucketName.empty() || applicationKey.empty()) {
            return nullptr;
        }

        static auto service = std::make_shared<B2Service>(keyId, applicationKey, bucketName);
        return service;
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

        auto b2Service = getB2Service();
        if (!b2Service) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody("B2 Service not configured");
            callback(resp);
            return;
        }

        auto &files = fileUpload.getFiles();
        auto shared_callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(std::move(callback));
        auto results = std::make_shared<Json::Value>();
        results->resize(0);
        auto remaining = std::make_shared<std::atomic<size_t>>(files.size());

        for (auto &file : files) {
            std::string fileName = file.getFileName();
            std::string fileContent(file.fileData(), file.fileLength());
            
            b2Service->upload(fileName, std::move(fileContent), [fileName, results, remaining, shared_callback](bool success, std::string fileId) {
                Json::Value res;
                res["fileName"] = fileName;
                res["success"] = success;
                res["fileId"] = fileId;
                
                static std::mutex resultsMutex;
                {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results->append(res);
                }

                if (--(*remaining) == 0) {
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(*results);
                    (*shared_callback)(resp);
                }
            });
        }
    }

    void Admin_Controller::b2Test(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto b2Service = getB2Service();
        if (!b2Service) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody("Error: B2 configuration missing or DB_API_KEY not set");
            callback(resp);
            return;
        }

        b2Service->runTestSequence([callback](bool success, std::string message) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody(message);
            callback(resp);
        });
    }
}
