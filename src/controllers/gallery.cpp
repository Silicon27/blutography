//
// Created by David Yang on 2026-01-11.
//
#include <controllers/gallery.hpp>
#include <support/gallery_storage.hpp>
#include <support/b2service.hpp>
#include <drogon/HttpAppFramework.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

namespace blutography {
    void GalleryController::get(const drogon::HttpRequestPtr& req, Callback_t callback) {
        auto resp = drogon::HttpResponse::newFileResponse(drogon::app().getDocumentRoot() + "/templates/gallery.html");
        callback(resp);
    }

    void GalleryController::get_data(const drogon::HttpRequestPtr& req, Callback_t callback) {
        auto items = GalleryStorage::instance().getAllItems();
        Json::Value root(Json::arrayValue);
        for (const auto& item : items) {
            Json::Value jItem;
            jItem["id"] = item.id;
            jItem["name"] = item.name;
            jItem["quote"] = item.quote;
            jItem["fileName"] = item.fileName;
            jItem["previewName"] = item.previewName;
            jItem["previewUrl"] = "/gallery_previews/" + item.previewName;
            jItem["imageUrl"] = "/gallery/image/" + item.id;
            jItem["metadata"]["dateTime"] = item.metadata.dateTime;
            jItem["metadata"]["model"] = item.metadata.model;
            jItem["metadata"]["exposure"] = item.metadata.exposure;
            jItem["metadata"]["iso"] = item.metadata.iso;
            jItem["metadata"]["width"] = item.metadata.width;
            jItem["metadata"]["height"] = item.metadata.height;
            root.append(jItem);
        }
        auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
        callback(resp);
    }

    void GalleryController::get_previews_bundle(const drogon::HttpRequestPtr& req, Callback_t callback) {
        // Generate a hash based on actual files in gallery_previews directory
        std::string hashInput;
        std::vector<std::string> previewFiles;

        if (std::filesystem::exists("gallery_previews") && std::filesystem::is_directory("gallery_previews")) {
            for (const auto& entry : std::filesystem::directory_iterator("gallery_previews")) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    previewFiles.push_back(filename);
                    hashInput += filename;
                    auto ftime = std::filesystem::last_write_time(entry.path());
                    auto duration = ftime.time_since_epoch();
                    auto count = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
                    hashInput += std::to_string(static_cast<long long>(count));
                }
            }
        }

        // Sort for consistent hash
        std::sort(previewFiles.begin(), previewFiles.end());

        if (previewFiles.empty()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("No preview images available");
            callback(resp);
            return;
        }

        std::string etag = "\"" + drogon::utils::getMd5(hashInput).substr(0, 16) + "\"";

        // Check If-None-Match header for cache validation
        auto ifNoneMatch = req->getHeader("If-None-Match");
        if (!ifNoneMatch.empty() && ifNoneMatch == etag) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k304NotModified);
            resp->addHeader("ETag", etag);
            resp->addHeader("Cache-Control", "private, max-age=31536000, immutable");
            callback(resp);
            return;
        }

        // Check if cached ZIP already exists
        std::string zipName = "previews_" + etag.substr(1, 16) + ".zip";
        std::string zipPath = "cache/" + zipName;

        if (!std::filesystem::exists(zipPath)) {
            // Create the ZIP bundle with lossless compression (store mode for already-compressed JPEGs)
            // Using -0 (store) for JPEGs since they're already compressed, -9 would waste CPU
            std::string cmd = "cd gallery_previews && zip -0 -q ../" + zipPath + " *";
            int res = std::system(cmd.c_str());

            if (res != 0) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k500InternalServerError);
                resp->setBody("Failed to create previews bundle");
                callback(resp);
                return;
            }
        }

        // Serve the ZIP file with caching headers
        auto resp = drogon::HttpResponse::newFileResponse(zipPath);
        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_ZIP);
        resp->addHeader("ETag", etag);
        resp->addHeader("Cache-Control", "private, max-age=31536000, immutable");
        resp->addHeader("Content-Disposition", "attachment; filename=previews.zip");
        callback(resp);
    }

    void GalleryController::get_preview_image(const drogon::HttpRequestPtr& req, Callback_t callback, const std::string& filename) {
        // Sanitize filename to prevent directory traversal
        if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos || filename.find("\\") != std::string::npos) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setBody("Invalid filename");
            callback(resp);
            return;
        }

        std::string filePath = "gallery_previews/" + filename;

        if (!std::filesystem::exists(filePath)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("Preview image not found");
            callback(resp);
            return;
        }

        auto resp = drogon::HttpResponse::newFileResponse(filePath);
        resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
        resp->addHeader("Cache-Control", "public, max-age=31536000, immutable");
        callback(resp);
    }

    void GalleryController::get_image(const drogon::HttpRequestPtr& req, Callback_t callback, const std::string& imageId) {
        auto optItem = GalleryStorage::instance().getItem(imageId);
        if (!optItem) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("Image not found");
            callback(resp);
            return;
        }

        auto item = *optItem;
        auto b2Service = B2Service::instance();
        if (!b2Service) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody("B2 Service not configured");
            callback(resp);
            return;
        }

        auto shared_callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
        b2Service->download(item.fileName, [shared_callback, item](bool success, std::string&& content) {
            if (success) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setBody(std::move(content));
                resp->setContentTypeCode(drogon::CT_IMAGE_JPG);
                resp->addHeader("Content-Disposition", "inline; filename=" + item.fileName);
                (*shared_callback)(resp);
            } else {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k500InternalServerError);
                resp->setBody("Failed to download image from storage");
                (*shared_callback)(resp);
            }
        });
    }

    void GalleryController::download_bundle(const drogon::HttpRequestPtr& req, Callback_t callback) {
        auto json = req->getJsonObject();
        if (!json || !(*json)["ids"].isArray()) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
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

        auto ids = (*json)["ids"];
        auto shared_callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(std::move(callback));
        auto remaining = std::make_shared<std::atomic<size_t>>(ids.size());
        
        std::string tempDir = "cache/bundle_" + drogon::utils::getUuid();
        std::filesystem::create_directories(tempDir);
        
        auto filePaths = std::make_shared<std::vector<std::string>>();
        auto mutex = std::make_shared<std::mutex>();

        if (ids.size() == 0) {
             auto resp = drogon::HttpResponse::newHttpResponse();
             resp->setStatusCode(drogon::k400BadRequest);
             (*shared_callback)(resp);
             return;
        }

        for (const auto& id : ids) {
            auto optItem = GalleryStorage::instance().getItem(id.asString());
            if (!optItem) {
                if (--(*remaining) == 0) goto finish;
                continue;
            }

            auto item = *optItem;
            b2Service->download(item.fileName, [item, tempDir, filePaths, mutex, remaining, shared_callback](bool success, std::string&& content) {
                if (success) {
                    std::string path = tempDir + "/" + item.fileName;
                    std::ofstream out(path, std::ios::binary);
                    if (out) {
                        out.write(content.data(), content.size());
                        std::lock_guard<std::mutex> lock(*mutex);
                        filePaths->push_back(item.fileName);
                    }
                }

                if (--(*remaining) == 0) {
                    // Finalize bundle - create lossless ZIP archive
                    std::string zipName = "bundle_" + drogon::utils::getUuid().substr(0, 8) + ".zip";
                    std::string zipPath = "cache/" + zipName;

                    // Use zip command with store mode (-0) for lossless or -9 for maximum compression
                    // -9 = maximum compression (still lossless for decompression)
                    // -j = junk paths (don't store directory structure)
                    // -q = quiet mode
                    std::string cmd = "cd " + tempDir + " && zip -9 -q ../" + zipName + " *";
                    int res = std::system(cmd.c_str());
                    
                    if (res == 0) {
                        auto resp = drogon::HttpResponse::newFileResponse(zipPath);
                        resp->setContentTypeCode(drogon::ContentType::CT_APPLICATION_ZIP);
                        resp->addHeader("Content-Disposition", "attachment; filename=" + zipName);
                        (*shared_callback)(resp);
                        // cleanup temp dir
                        std::filesystem::remove_all(tempDir);
                        // ZIP file will be cleaned up by cache cleanup on shutdown
                    } else {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k500InternalServerError);
                        resp->setBody("Failed to create ZIP bundle");
                        (*shared_callback)(resp);
                        std::filesystem::remove_all(tempDir);
                    }
                }
            });
        }
        return;

        finish:
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k404NotFound);
        (*shared_callback)(resp);
    }
}