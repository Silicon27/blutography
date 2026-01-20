#ifndef BLUTOGRAPHY_B2SERVICE_HPP
#define BLUTOGRAPHY_B2SERVICE_HPP

#include <drogon/drogon.h>
#include <string>
#include <functional>
#include <memory>

#include <mutex>
#include <chrono>

namespace blutography {

struct B2AuthResponse {
    std::string accountId;
    std::string apiUrl;
    std::string authorizationToken;
    std::string downloadUrl;
    std::string bucketId;
};

struct B2UploadData {
    std::string uploadUrl;
    std::string uploadAuthToken;
    std::chrono::steady_clock::time_point lastUpdated;
};

class B2Service : public std::enable_shared_from_this<B2Service> {
public:
    static std::shared_ptr<B2Service> instance();
    B2Service(std::string keyId, std::string applicationKey, std::string bucketName);

    // Runs Ping (Authorize), Upload, and Delete sequence
    void runTestSequence(std::function<void(bool success, std::string message)>&& callback);

    // High-level upload method with caching
    void upload(const std::string& fileName, 
                std::string&& content, 
                std::function<void(bool success, std::string fileId)>&& callback);

    // High-level download method
    void download(const std::string& fileName,
                  std::function<void(bool success, std::string&& content)>&& callback);

private:
    std::string keyId_;
    std::string applicationKey_;
    std::string bucketName_;

    // Caching
    std::mutex mutex_;
    std::optional<B2AuthResponse> authCache_;
    std::chrono::steady_clock::time_point lastAuthTime_;
    std::optional<B2UploadData> uploadCache_;

    void getAuth(std::function<void(bool success, B2AuthResponse auth)>&& callback);
    void getUpload(const B2AuthResponse& auth, std::function<void(bool success, B2UploadData uploadData)>&& callback);

    void authorize(std::function<void(bool success, B2AuthResponse auth)>&& callback);
    
    void getUploadUrl(const B2AuthResponse& auth, 
                      std::function<void(bool success, std::string uploadUrl, std::string uploadAuthToken)>&& callback);
    
    void uploadFile(const std::string& uploadUrl, 
                    const std::string& uploadAuthToken, 
                    const std::string& fileName, 
                    std::string&& content, 
                    std::function<void(bool success, std::string fileId)>&& callback);
    
    void deleteFile(const B2AuthResponse& auth, 
                    const std::string& fileName, 
                    const std::string& fileId, 
                    std::function<void(bool success)>&& callback);
};

}

#endif // BLUTOGRAPHY_B2SERVICE_HPP
