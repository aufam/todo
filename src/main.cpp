#include <boost/preprocessor.hpp>
#include <fmt/chrono.h>
#include <delameta/debug.h>
#include <delameta/tcp.h>
#include <delameta/http/http.h>
#include <delameta/opts.h>
#include <catch2/catch_session.hpp>
#include <csignal>

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

OPTS_MAIN(
    (TODO, "Simple todo list")
    ,
    //  |    Type   |   Name  | Short |    Long   |              Help               |     Default     |
        (URL        , host    ,  'H'  , "host"    , "Specify host to serve HTTP"    , "localhost:5000")
        (int        , max_sock,  'j'  , "max-sock", "Set number of server socket"   , "4"             )
        (bool       , verbose ,  'v'  , "verbose" , "Set verbosity"                                   )
        (bool       , version ,  'V'  , "version" , "Print version"                                   )
        (bool       , test    ,  't'  , "test"    , "Enable testing"                                  )
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

    app.logger = [](const std::string& ip, const http::RequestReader& req, const http::ResponseWriter& res) {
        fmt::println("{:%Y-%m-%d %H:%M:%S} {} {} {} {}", format_time_now(), ip, req.method, req.url.full_path, res.status);
    };

    Server<TCP> server;
    app.bind(server, {.is_tcp_server=true});

    fmt::println("Starting server on {}", host.host);
    on_sigint([&]() { server.stop(); });

    return server.start(Server<TCP>::Args{
        .host=host.host, 
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

