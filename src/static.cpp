#include <boost/preprocessor.hpp>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/file.h>
#include <delameta/utils.h>
#include <dirent.h>

using namespace Project;
using delameta::File;
using delameta::panic;
using etl::Ok;
using etl::Err;
using etl::Ref;
namespace http = delameta::http;

HTTP_SETUP(static_router, app) {
    DIR* dir = ::opendir(HOME_DIR "/static");
    if (dir == nullptr) {
        panic(FL, "Error opening static directory");
    }

    struct dirent* entry;
    static std::list<std::string> items;
    while ((entry = ::readdir(dir)) != nullptr) {
        items.push_back(std::string("/static/") + entry->d_name);
    }
    
    ::closedir(dir);

    dir = ::opendir(HOME_DIR "/assets");
    if (dir == nullptr) {
        panic(FL, "Error opening static directory");
    }

    while ((entry = ::readdir(dir)) != nullptr) {
        items.push_back(std::string("/assets/") + entry->d_name);
    }
    
    ::closedir(dir);

    auto load_file = [](Ref<http::ResponseWriter> res, const std::string& filename) -> http::Result<void> {
        auto file = TRY(File::Open(File::Args{.filename=filename.c_str(), .mode="r"}));

        res->headers["Content-Type"] = delameta::get_content_type_from_file(filename);
        res->headers["Content-Length"] = std::to_string(file.file_size());
        file >> res->body_stream;

        return Ok();
    };

    for (const auto& item: items) {
        app.route(item, {"GET"}, std::tuple{http::arg::response},
        [&](Ref<http::ResponseWriter> res) {
            return load_file(res, HOME_DIR + item);
        });
    }

    app.route("/", {"GET"}, std::tuple{http::arg::response},
    [&](Ref<http::ResponseWriter> res) {
        return load_file(res, HOME_DIR "/static/index.html");
    });
}

