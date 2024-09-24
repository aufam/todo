#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>
#include <delameta/utils.h>
#include <dirent.h>
#include <algorithm>

using namespace Project;
using delameta::File;
using delameta::panic;
using etl::Ok;
using etl::Err;
using etl::Ref;
namespace http = delameta::http;

void static_refresh();

HTTP_SETUP(static_setup, app) {
    static_refresh();
}

HTTP_ROUTE(
    ("/static/load", ("GET")),
    (load_file),
        (Ref<http::ResponseWriter>, res     , http::arg::response       )
        (std::string              , filename, http::arg::arg("filename")),
    (http::Result<void>)
) {
    auto file = TRY(File::Open(File::Args{.filename=filename.c_str(), .mode="r"}));

    res->headers["Content-Type"] = delameta::get_content_type_from_file(filename);
    res->headers["Content-Length"] = std::to_string(file.file_size());
    file >> res->body_stream;

    return Ok();
}

HTTP_ROUTE(
    ("/", ("GET")),
    (static_index),
        (Ref<http::ResponseWriter>, res, http::arg::response),
    (http::Result<void>)
) {
    return load_file(res, HOME_DIR "/static/index.html");
}

HTTP_ROUTE(
    ("/static/refresh", ("GET")),
    (static_refresh),,
    (void)
) {
    static std::list<std::string> paths;
    for (const auto& path: paths) {
        auto it = std::find_if(app.routers.begin(), app.routers.end(), [&](http::Router& r) {
            return r.path == path;
        });

        if (it != app.routers.end()) app.routers.erase(it);
    }

    paths.clear();

    auto paths_append_from = [](const char* directory, const std::string& prefix) {
        DIR* dir = ::opendir(directory);
        if (dir == nullptr) {
            panic(FL, "Error opening static directory");
        }

        struct dirent* entry;
        while ((entry = ::readdir(dir)) != nullptr) {
            paths.push_back(prefix + entry->d_name);
        }

        ::closedir(dir);
    };

    paths_append_from(HOME_DIR "/static", "/static/");
    paths_append_from(HOME_DIR "/assets", "/assets/");

    for (const auto& path: paths) {
        app.route(path, {"GET"}, std::tuple{http::arg::response},
        [=](Ref<http::ResponseWriter> res) {
            return load_file(res, HOME_DIR + path);
        });
    }
}

