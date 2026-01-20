#include <controllers/admin.hpp>
#include <filters/adminfilter.hpp>
#include <support/b2service.hpp>
#include <support/image_utils.hpp>
#include <support/gallery_storage.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/MultiPart.h>
#include <drogon/utils/Utilities.h>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <thread>

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

    void Admin_Controller::uploadImage(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        drogon::MultiPartParser fileUpload;
        if (fileUpload.parse(req) != 0 || fileUpload.getFiles().empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setBody("Invalid upload request");
            callback(resp);
            return;
        }

        auto b2Service = B2Service::instance();
        if (!b2Service) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody("B2 Service not configured");
            callback(resp);
            return;
        }

        auto &files = fileUpload.getFiles();
        auto &params = fileUpload.getParameters();
        
        std::string reqName = params.count("name") ? params.at("name") : "";
        std::string reqQuote = params.count("quote") ? params.at("quote") : "";

        auto shared_callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(std::move(callback));
        auto results = std::make_shared<Json::Value>();
        results->resize(0);
        auto remaining = std::make_shared<std::atomic<size_t>>(files.size());

        for (auto &file : files) {
            std::string fileName = file.getFileName();
            // Copy data to a shared buffer for background processing
            auto fileContent = std::make_shared<std::string>(file.fileData(), file.fileLength());
            
            // Generate unique ID (hash)
            std::string imageId = drogon::utils::getSha1(*fileContent).substr(0, 12);
            std::string name = reqName.empty() ? fileName : reqName;
            std::string quote = reqQuote;

            // Offload CPU-intensive compression to a background thread to keep IO loop free
            std::thread([imageId, name, quote, fileName, fileContent, b2Service, results, remaining, shared_callback]() {
                // 1. Extract Metadata
                image::Metadata metadata = image::extractMetadata(*fileContent);

                // 2. Generate and save preview locally for the server-side gallery
                std::string previewName = fileName;
                try {
                    std::string previewData = image::createGalleryPreview(*fileContent, fileName);
                    if (!previewData.empty()) {
                        size_t lastDot = previewName.find_last_of(".");
                        if (lastDot != std::string::npos) {
                            previewName = previewName.substr(0, lastDot) + ".jpg";
                        } else {
                            previewName += ".jpg";
                        }

                        std::string previewPath = "gallery_previews/" + previewName;
                        std::ofstream out(previewPath, std::ios::binary);
                        if (out) {
                            out.write(previewData.data(), previewData.size());
                            LOG_DEBUG << "Gallery preview saved: " << previewPath;
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR << "Gallery preview generation failed for " << fileName << ": " << e.what();
                }

                // 3. Store in GalleryStorage
                GalleryItem item;
                item.id = imageId;
                item.name = name;
                item.quote = quote;
                item.fileName = fileName;
                item.previewName = previewName;
                item.metadata = metadata;
                GalleryStorage::instance().addItem(item);

                // 4. Upload original (lossless) to Backblaze B2 database
                b2Service->upload(fileName, std::move(*fileContent), [fileName, results, remaining, shared_callback](bool success, std::string fileId) {
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
                        // All files in this batch processed
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(*results);
                        (*shared_callback)(resp);
                    }
                });
            }).detach();
        }
    }

    void Admin_Controller::b2Test(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        auto b2Service = B2Service::instance();
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
