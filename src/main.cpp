#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/tcp.h>
#include <delameta/http/http.h>
#include <delameta/opts.h>
#include <catch2/catch_session.hpp>
#include <csignal>
#include <algorithm>

HTTP_DEFINE_OBJECT(app);

using namespace Project;
using delameta::URL;
using delameta::TCP;
using delameta::Server;
using delameta::Result;
using delameta::Opts;
using etl::Ok;
using etl::Err;
namespace http = delameta::http;

extern void users_create_table(const char* path);
extern void todos_create_table(const char* path);

static void on_sigint(std::function<void()> fn);
static std::tm format_time_now();

using Headers = std::unordered_map<std::string_view, std::string_view>;
using Queries = std::unordered_map<std::string, std::string>;

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Headers> {
    if (str == "") return Ok(Headers{});
    return json::deserialize<Headers>(str).except([](const char* err) { return Error{-1, err}; });
}

template<>
auto Opts::convert_string_into(const std::string& str) -> Result<Queries> {
    if (str == "") return Ok(Queries{});
    return json::deserialize<Queries>(str).except([](const char* err) { return Error{-1, err}; });
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
        (Queries    , queries ,  'q'  , "queries" , "Specify URL queries"           , ""              )
        (Headers    , headers ,  'a'  , "headers" , "Specify HTTP headers"          , ""              )
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
        const char* argv[] = {"./test", "--order", "lex", ""};
        const auto argc = sizeof(argv) / sizeof(size_t);
        if (verbose) argv[argc - 1] = "--success";

        Catch::Session session;
        int res = session.run(argc, argv);

        if (res != 0) {
            return Err("Test failed");
        }

        return Ok();
    }

    users_create_table(nullptr);
    todos_create_table(nullptr);

    if (not route.empty()) {
        if (method.empty()) method = body.empty() ? "GET" : "POST";
        auto it = std::find_if(app.routers.begin(), app.routers.end(), [&](const http::Router& r) {
            return r.path == route && std::find(r.methods.begin(), r.methods.end(), method) != r.methods.end();
        });
        if (it == app.routers.end()) {
            return Err(delameta::Error{-1, fmt::format("cannot find route '{}'", route)});
        }

        http::RequestReader req;
        http::ResponseWriter res;

        req.version = "HTTP/1.1";
        req.method = !method.empty() ? method : body.empty() ? "GET" : "POST";
        req.url = std::move(uri);
        req.url.path = route;
        req.url.queries = std::move(queries);
        req.headers = std::move(headers);

        std::string content_length;
        req.headers["Content-Length"] = content_length = std::to_string(body.size());
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

        req.body_stream << body;

        res.status = 200;
        it->function(req, res);
        res.body_stream >> [&](std::string_view sv) {
            res.body += sv;
        };

        fmt::println("{}", res.body);
        if (res.status < 300) {
            return Ok();
        } else {
            return Err(delameta::Error{res.status, http::status_to_string(res.status)});
        }
    }

    app.logger = [](const std::string& ip, const http::RequestReader& req, const http::ResponseWriter& res) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {} {} {} {}", format_time_now(), ip, req.method, req.url.full_path, res.status);
    };

    Server<TCP> server;
    app.bind(server, {.is_tcp_server=true});

    fmt::println("Starting server on {}", uri.host);
    on_sigint([&]() { server.stop(); });

    return server.start(Server<TCP>::Args{
        .host=uri.host, 
        .max_socket=max_sock
    });
}

static void on_sigint(std::function<void()> fn) {
    static std::function<void()> at_exit;
    at_exit = std::move(fn);
    auto sig = +[](int) { at_exit(); };
    std::signal(SIGINT, sig);
    std::signal(SIGTERM, sig);
    std::signal(SIGQUIT, sig);
}

static std::tm format_time_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);
    return fmt::localtime(time_now);
}

void delameta::info(const char* file, int line, const std::string& msg) {
    if (not Opts::verbose) return;
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} INFO: {}", format_time_now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} INFO: {}", format_time_now(), file, line, msg);
    }
}

void delameta::warning(const char* file, int line, const std::string& msg) {
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} WARNING: {}", format_time_now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} WARNING: {}", format_time_now(), file, line, msg);
    }
}

void delameta::panic(const char* file, int line, const std::string& msg) {
    if (line == 0) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} PANIC: {}", format_time_now(), msg);
    } else {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {}:{} PANIC: {}", format_time_now(), file, line, msg);
    }
    exit(1);
}

