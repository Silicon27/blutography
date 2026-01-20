cmake_minimum_required(VERSION 3.15)
project(blutography CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies
find_package(Drogon CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)

# TurboJPEG
find_path(TURBOJPEG_INCLUDE_DIR NAMES turbojpeg.h PATHS /opt/homebrew/include /usr/local/include)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg PATHS /opt/homebrew/lib /usr/local/lib)

if(NOT TURBOJPEG_INCLUDE_DIR OR NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "TurboJPEG not found!")
endif()

# Source Files
set(SRC_FILES
    src/main.cpp
    src/controllers/home.cc
    src/controllers/gallery.cpp
    src/controllers/admin.cpp
    src/support/b2service.cpp
    src/support/image_utils.cpp
    src/support/gallery_storage.cpp
    src/filters/adminfilter.cpp
)

# Executable
add_executable(blutography ${SRC_FILES})

# Views
drogon_create_views(blutography src/views ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(blutography
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TURBOJPEG_INCLUDE_DIR}
    src/models
    include
)

target_link_libraries(blutography PRIVATE 
    Drogon::Drogon 
    yaml-cpp::yaml-cpp 
    ${TURBOJPEG_LIBRARY}
)

# Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
endif()
