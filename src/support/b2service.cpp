#include <support/b2service.hpp>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <ctime>

namespace blutography {

static void splitUrl(const std::string& url, std::string& host, std::string& path) {
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        host = url;
        path = "/";
        return;
    }
    pos += 3;
    size_t pathPos = url.find("/", pos);
    if (pathPos == std::string::npos) {
        host = url;
        path = "/";
    } else {
        host = url.substr(0, pathPos);
        path = url.substr(pathPos);
    }
}

B2Service::B2Service(std::string keyId, std::string applicationKey, std::string bucketName)
    : keyId_(std::move(keyId)), applicationKey_(std::move(applicationKey)), bucketName_(std::move(bucketName)) {}

std::shared_ptr<B2Service> B2Service::instance() {
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

void B2Service::upload(const std::string& fileName, std::string&& content, std::function<void(bool success, std::string fileId)>&& callback) {
    auto self = shared_from_this();
    auto sharedContent = std::make_shared<std::string>(std::move(content));
    getAuth([self, fileName, sharedContent, callback = std::move(callback)](bool success, B2AuthResponse auth) mutable {
        if (!success) {
            callback(false, "");
            return;
        }
        self->getUpload(auth, [self, auth, fileName, sharedContent, callback = std::move(callback)](bool success, B2UploadData uploadData) mutable {
            if (!success) {
                callback(false, "");
                return;
            }
            
            // We need a copy of the content for the first attempt because uploadFile will move it
            // Optimization: If the file is small, copy is fine. If large, it's still safer for retry.
            std::string contentForAttempt = *sharedContent;
            
            self->uploadFile(uploadData.uploadUrl, uploadData.uploadAuthToken, fileName, std::move(contentForAttempt), [self, auth, fileName, sharedContent, callback = std::move(callback)](bool success, std::string fileId) mutable {
                if (!success) {
                    LOG_WARN << "B2 Upload failed with cached URL, retrying with fresh URL...";
                    {
                        std::lock_guard<std::mutex> lock(self->mutex_);
                        self->uploadCache_.reset();
                    }
                    self->getUpload(auth, [self, fileName, sharedContent, callback = std::move(callback)](bool success, B2UploadData uploadData) mutable {
                        if (!success) {
                            callback(false, "");
                            return;
                        }
                        // Second attempt: move the original shared content
                        self->uploadFile(uploadData.uploadUrl, uploadData.uploadAuthToken, fileName, std::move(*sharedContent), std::move(callback));
                    });
                } else {
                    callback(true, fileId);
                }
            });
        });
    });
}

void B2Service::download(const std::string& fileName, std::function<void(bool success, std::string&& content)>&& callback) {
    auto self = shared_from_this();
    getAuth([self, fileName, callback = std::move(callback)](bool success, B2AuthResponse auth) mutable {
        if (!success) {
            callback(false, "");
            return;
        }

        std::string downloadUrl = auth.downloadUrl + "/file/" + self->bucketName_ + "/" + fileName;
        std::string host, path;
        splitUrl(downloadUrl, host, path);

        auto client = drogon::HttpClient::newHttpClient(host);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setPath(path);
        req->setMethod(drogon::Get);
        req->addHeader("Authorization", auth.authorizationToken);

        client->sendRequest(req, [callback = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) mutable {
            if (result == drogon::ReqResult::Ok && resp->statusCode() == drogon::k200OK) {
                std::string body(resp->body().data(), resp->body().size());
                callback(true, std::move(body));
            } else {
                LOG_ERROR << "B2 Download failed for " << result << " Status: " << (resp ? (int)resp->statusCode() : 0);
                callback(false, "");
            }
        });
    });
}

void B2Service::getAuth(std::function<void(bool success, B2AuthResponse auth)>&& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (authCache_.has_value()) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::hours>(now - lastAuthTime_).count() < 20) {
                callback(true, *authCache_);
                return;
            }
        }
    }

    auto self = shared_from_this();
    authorize([self, callback = std::move(callback)](bool success, B2AuthResponse auth) mutable {
        if (success) {
            std::lock_guard<std::mutex> lock(self->mutex_);
            self->authCache_ = auth;
            self->lastAuthTime_ = std::chrono::steady_clock::now();
        }
        callback(success, auth);
    });
}

void B2Service::getUpload(const B2AuthResponse& auth, std::function<void(bool success, B2UploadData uploadData)>&& callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (uploadCache_.has_value()) {
            auto now = std::chrono::steady_clock::now();
            // Refresh upload URL every 12 hours (B2 tokens last 24h, but URLs can be shorter lived if many files uploaded)
            if (std::chrono::duration_cast<std::chrono::hours>(now - uploadCache_->lastUpdated).count() < 12) {
                callback(true, *uploadCache_);
                return;
            }
        }
    }

    auto self = shared_from_this();
    getUploadUrl(auth, [self, callback = std::move(callback)](bool success, std::string uploadUrl, std::string uploadAuthToken) mutable {
        B2UploadData data;
        if (success) {
            data.uploadUrl = uploadUrl;
            data.uploadAuthToken = uploadAuthToken;
            data.lastUpdated = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(self->mutex_);
            self->uploadCache_ = data;
        }
        callback(success, data);
    });
}

void B2Service::runTestSequence(std::function<void(bool success, std::string message)>&& callback) {
    auto self = shared_from_this();
    authorize([self, callback = std::move(callback)](bool success, B2AuthResponse auth) mutable {
        if (!success) {
            callback(false, "B2 Authorization failed");
            return;
        }

        LOG_INFO << "B2 Authorization successful. API URL: " << auth.apiUrl;

        self->getUploadUrl(auth, [self, auth, callback = std::move(callback)](bool success, std::string uploadUrl, std::string uploadAuthToken) mutable {
            if (!success) {
                callback(false, "B2 Get Upload URL failed");
                return;
            }

            std::string testFileName = "ping_" + std::to_string(std::time(nullptr)) + ".txt";
            std::string testContent = "Ping from Blutography Backend at " + std::to_string(std::time(nullptr));

            self->uploadFile(uploadUrl, uploadAuthToken, testFileName, std::move(testContent), [self, auth, testFileName, callback = std::move(callback)](bool success, std::string fileId) mutable {
                if (!success) {
                    callback(false, "B2 Upload failed");
                    return;
                }

                LOG_INFO << "B2 Upload successful. File Name: " << testFileName << ", File ID: " << fileId;

                self->deleteFile(auth, testFileName, fileId, [callback = std::move(callback)](bool success) mutable {
                    if (!success) {
                        callback(false, "B2 Delete failed");
                        return;
                    }
                    callback(true, "B2 Ping-Upload-Delete sequence completed successfully!");
                });
            });
        });
    });
}

void B2Service::authorize(std::function<void(bool success, B2AuthResponse auth)>&& callback) {
    auto client = drogon::HttpClient::newHttpClient("https://api.backblazeb2.com");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath("/b2api/v2/b2_authorize_account");
    req->setMethod(drogon::Get);
    
    std::string authStr = keyId_ + ":" + applicationKey_;
    req->addHeader("Authorization", "Basic " + drogon::utils::base64Encode((const unsigned char*)authStr.data(), authStr.size()));

    client->sendRequest(req, [callback = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        if (result != drogon::ReqResult::Ok || !resp || resp->statusCode() != drogon::k200OK) {
            LOG_ERROR << "B2 Auth Error: " << (resp ? std::to_string(resp->statusCode()) : "No response");
            if (resp) LOG_ERROR << "Body: " << resp->body();
            callback(false, {});
            return;
        }

        auto json = resp->getJsonObject();
        if (!json) {
            callback(false, {});
            return;
        }

        B2AuthResponse auth;
        auth.accountId = (*json)["accountId"].asString();
        auth.apiUrl = (*json)["apiUrl"].asString();
        auth.authorizationToken = (*json)["authorizationToken"].asString();
        auth.downloadUrl = (*json)["downloadUrl"].asString();
        auth.bucketId = (*json)["allowed"]["bucketId"].asString();
        callback(true, auth);
    });
}

void B2Service::getUploadUrl(const B2AuthResponse& auth, std::function<void(bool success, std::string uploadUrl, std::string uploadAuthToken)>&& callback) {
    auto client = drogon::HttpClient::newHttpClient(auth.apiUrl);
    Json::Value body;
    body["bucketId"] = auth.bucketId;
    auto req = drogon::HttpRequest::newHttpJsonRequest(body);
    req->setPath("/b2api/v2/b2_get_upload_url");
    req->setMethod(drogon::Post);
    req->addHeader("Authorization", auth.authorizationToken);

    client->sendRequest(req, [callback = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        if (result != drogon::ReqResult::Ok || !resp || resp->statusCode() != drogon::k200OK) {
            LOG_ERROR << "B2 GetUploadUrl Error: " << (resp ? resp->body() : "No response");
            callback(false, "", "");
            return;
        }
        auto json = resp->getJsonObject();
        callback(true, (*json)["uploadUrl"].asString(), (*json)["authorizationToken"].asString());
    });
}

void B2Service::uploadFile(const std::string& uploadUrl, const std::string& uploadAuthToken, const std::string& fileName, std::string&& content, std::function<void(bool success, std::string fileId)>&& callback) {
    std::string host, path;
    splitUrl(uploadUrl, host, path);
    auto client = drogon::HttpClient::newHttpClient(host);
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(path);
    req->setMethod(drogon::Post);
    req->addHeader("Authorization", uploadAuthToken);
    req->addHeader("X-Bz-File-Name", drogon::utils::urlEncode(fileName));
    req->addHeader("Content-Type", "b2/x-auto");
    req->addHeader("X-Bz-Content-Sha1", drogon::utils::getSha1(content));
    req->setBody(std::move(content));

    client->sendRequest(req, [callback = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        if (result != drogon::ReqResult::Ok || !resp || resp->statusCode() != drogon::k200OK) {
            LOG_ERROR << "B2 Upload Error: " << (resp ? resp->body() : "No response");
            callback(false, "");
            return;
        }
        auto json = resp->getJsonObject();
        callback(true, (*json)["fileId"].asString());
    });
}

void B2Service::deleteFile(const B2AuthResponse& auth, const std::string& fileName, const std::string& fileId, std::function<void(bool success)>&& callback) {
    auto client = drogon::HttpClient::newHttpClient(auth.apiUrl);
    Json::Value body;
    body["fileName"] = fileName;
    body["fileId"] = fileId;
    auto req = drogon::HttpRequest::newHttpJsonRequest(body);
    req->setPath("/b2api/v2/b2_delete_file_version");
    req->setMethod(drogon::Post);
    req->addHeader("Authorization", auth.authorizationToken);

    client->sendRequest(req, [callback = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
        if (result != drogon::ReqResult::Ok || !resp || resp->statusCode() != drogon::k200OK) {
            LOG_ERROR << "B2 Delete Error: " << (resp ? resp->body() : "No response");
            callback(false);
            return;
        }
        callback(true);
    });
}

}
