//
// Created by David Yang on 2026-01-20.
//

#ifndef BLUTOGRAPHY_ADMINFILTER_HPP
#define BLUTOGRAPHY_ADMINFILTER_HPP

#include <drogon/HttpFilter.h>
#include <drogon/HttpResponse.h>

namespace blutography {
    class AdminAuthFilter : public drogon::HttpFilter<AdminAuthFilter> {
    public:
        void doFilter(const drogon::HttpRequestPtr &req,
                      drogon::FilterCallback &&fcb,
                      drogon::FilterChainCallback &&fccb) override {
            auto session = req->getSession();
            if (session && session->getOptional<bool>("is_admin").value_or(false)) {
                // Admin session found, proceed to the next filter or the handler
                fccb();
                return;
            }
            // Not an admin, redirect to login page
            auto resp = drogon::HttpResponse::newRedirectionResponse("/login");
            fcb(resp);
        }
    };
}

#endif //BLUTOGRAPHY_ADMINFILTER_HPP