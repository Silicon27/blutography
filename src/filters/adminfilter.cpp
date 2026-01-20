#include <filters/adminfilter.hpp>
#include <drogon/HttpAppFramework.h>

// Force instantiation of DrObject to register the filter
template class drogon::DrObject<blutography::AdminAuthFilter>;
