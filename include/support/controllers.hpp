//
// Created by David Yang on 2026-01-10.
//

#ifndef BLUTOGRAPHY_CONTROLLERS_HPP
#define BLUTOGRAPHY_CONTROLLERS_HPP
#include <drogon/drogon.h>

namespace blutography {
    using Callback_t = std::function<void(const drogon::HttpResponsePtr &)>&&;
}

#endif //BLUTOGRAPHY_CONTROLLERS_HPP