#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/http/http.h>
#include <delameta/opts.h>
#include <catch2/catch_session.hpp>
#include <csignal>
#include <algorithm>

HTTP_DEFINE_OBJECT(app);

using namespace Project;
using delameta::URL;
using delameta::Server;
using delameta::Result;
using delameta::Opts;
using etl::Ok;
using etl::Err;
namespace http = delameta::http;

extern void users_create_table(const char* path = nullptr);
extern void todos_create_table(const char* path = nullptr);

using Args = std::unordered_map<std::string, std::string>;

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Args> {
    if (str == "") return Ok(Args{});
    return json::deserialize<Args>(str).except([](const char* err) { return Error{-1, err}; });
}

static auto now() {
    return std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
}

OPTS_MAIN(
    (TODO, "Simple todo list")
    ,
    //  |    Type   |   Name  | Short |    Long   |              Help               |     Default     |
        (URL        , uri     ,  'H'  , "host"    , "Specify host to serve HTTP"    , "localhost:5000")
        (int        , max_sock,  'n'  , "max-sock", "Set number of server socket"   , "4"             )
        (bool       , verbose ,  'v'  , "verbose" , "Set verbosity"                                   )
        (bool       , version ,  'V'  , "version" , "Print version"                                   )
        (bool       , test    ,  't'  , "test"    , "Enable testing"                                  )
        (std::string, route   ,  'r'  , "route"   , "Execute HTTP route"            , ""              )
        (std::string, method  ,  'm'  , "method"  , "Specify HTTP method"           , ""              )
        (Args       , headers ,  'a'  , "headers" , "Specify HTTP headers"          , ""              )
        (std::string, body    ,  'd'  , "body"    , "Specify HTTP body"             , ""              )
        (std::string, token   ,  'T'  , "token"   , "Specify access token"          , ""              )
        (bool       , is_json ,  'j'  , "is-json" , "Set data type to be json"                        )
        (bool       , is_text ,  'x'  , "is-text" , "Set data type to be plain text"                  )
        (bool       , is_form ,  'f'  , "is-form" , "Set data type to be form-urlencoded"             )
    ,
    (Result<void>)
) {
    if (version) {
        fmt::println("delameta: v" DELAMETA_VERSION);
        return Ok();
    }

    Opts::verbose = verbose;

    if (test) {
        const char* argv[] = {"test", "--order", "lex", ""};
        const auto argc = sizeof(argv) / sizeof(size_t);
        if (verbose) argv[argc - 1] = "--success";

        Catch::Session session;
        int res = session.run(argc, argv);

        if (res != 0) {
            return Err("Test failed");
        }

        return Ok();
    }

    users_create_table();
    todos_create_table();

    if (not route.empty()) {
        class DummyClient : public delameta::StreamSessionClient {
        public:
            http::Http& http;
            delameta::StringStream ss;
            DummyClient(http::Http& http) : StreamSessionClient(ss), http(http), ss() {}

            delameta::Result<std::vector<uint8_t>> request(delameta::Stream& in_stream) override {
                in_stream >> ss;
                auto [req, res] = http.execute(ss);
                ss.flush();
                res.dump() >> ss;
                return Ok(std::vector<uint8_t>());
            }
        };
        DummyClient dummy_client(app);

        http::RequestWriter req;
        if (method.empty()) req.method = body.empty() ? "GET" : "POST";
        req.version = "HTTP/1.1";
        req.url = uri.url + route;
        req.headers = std::move(headers);

        req.headers["Content-Length"] = std::to_string(body.size());
        if (is_json) {
            req.headers["Content-Type"] = "application/json";
        } else if (is_text) {
            req.headers["Content-Type"] = "text/plain";
        } else if (is_form) {
            req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        }
        if (not token.empty()) {
            req.headers["Authentication"] = token;
        }

        auto res = delameta::http::request(dummy_client, std::move(req));
        if (res.is_err()) {
            return Err(std::move(res.unwrap_err()));
        }

        auto &response = res.unwrap();
        response.body_stream >> [&](std::string_view sv) {
            response.body += sv;
        };

        fmt::println("{}", response.body);
        if (response.status < 300) {
            return Ok();
        } else {
            return Err(delameta::Error{response.status, http::status_to_string(response.status)});
        }
    }

    app.logger = [](const std::string& ip, const http::RequestReader& req, const http::ResponseWriter& res) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {} {} {} {}", now(), ip, req.method, req.url.full_path, res.status);
    };

    fmt::println("Server is running on {}", uri.host);
    return app.listen(http::Http::ListenArgs{
        .host=uri.host,
        .max_socket=max_sock
    });
}

void delameta::info(const char* file, int line, const std::string& msg) {
    if (not Opts::verbose) return;
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} INFO: {}", now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} INFO: {}", now(), file, line, msg);
    }
}

void delameta::warning(const char* file, int line, const std::string& msg) {
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} WARNING: {}", now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} WARNING: {}", now(), file, line, msg);
    }
}

void delameta::panic(const char* file, int line, const std::string& msg) {
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} PANIC: {}", now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} PANIC: {}", now(), file, line, msg);
    }
    exit(1);
}

