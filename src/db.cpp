#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/sqlite3/connection.h>
#include <delameta/http/http.h>
#include <catch2/catch_test_macros.hpp>

using namespace Project::delameta;
using sql_db = sqlpp::sqlite3::connection;
const auto DB_PATH = HOME_DIR "/assets/database.db";

auto db_open(const char* path) -> sqlpp::sqlite3::connection {
    sqlpp::sqlite3::connection_config config;
    config.path_to_database = path ? path : DB_PATH;
    config.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    config.debug = true;
    return sql_db(config);
}

auto db_dependency(const http::RequestReader& req, http::ResponseWriter&) -> sql_db {
    auto it = req.url.queries.find("db-path");
    return db_open(it == req.url.queries.end() ? nullptr : it->second.c_str());
}

TEST_CASE("9. cleanup db", "[cleanup]") {
    ::remove("test.db");
}