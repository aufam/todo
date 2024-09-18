#include <boost/preprocessor.hpp>
#include <delameta/http/http.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/ppgen.h>
#include <catch2/catch_test_macros.hpp>
#include "chrono.h"

using namespace Project;
using etl::Ok;
using etl::Err;
using etl::defer;
using time_point = std::chrono::system_clock::time_point;
using sql_db = sqlpp::sqlite3::connection;
namespace http = delameta::http;

extern auto db_open(const char*) -> sql_db;
extern auto db_dependency(const http::RequestReader&, http::ResponseWriter&) -> sql_db;
extern auto jwt_create_token(const std::vector<std::pair<std::string, std::string>>& payload) -> std::string;
extern auto jwt_decode_token(const std::string& token) -> delameta::Result<std::string>;
extern auto password_hash(const std::string& password) -> std::string;
extern void todos_delete(uint64_t user_id, sql_db db);

SQLPP_DECLARE_TABLE(
    (Users)
    ,
    (id        , int         , SQLPP_PRIMARY_KEY)
    (username  , varchar(32) , SQLPP_NOT_NULL   )
    (password  , varchar(128), SQLPP_NOT_NULL   )
    (created_at, timestamp   , SQLPP_NOT_NULL   )
)

static const Users::Users users;

auto users_create_table(const char* path) -> sql_db {
    auto db = db_open(path);
    db.execute(R"(CREATE TABLE IF NOT EXISTS Users (
        id         INTEGER        PRIMARY KEY,
        username   VARCHAR(32)    UNIQUE NOT NULL,
        password   VARCHAR(128)   NOT NULL,
        created_at TIMESTAMP      DEFAULT CURRENT_TIMESTAMP
    ))");

    return db;
}

HTTP_SETUP(users_setup, app) {
    users_create_table(nullptr);
}

JSON_DECLARE(
    (User)
    ,
    (std::string, username  )
    (time_point , created_at)
)

JSON_DECLARE(
    (UserForm)
    ,
    (std::string, username)
    (std::string, password)
)

HTTP_ROUTE(
    ("/user/create-token", ("POST")),
    (user_create_token),
        (UserForm, user, http::arg::json),
    (std::string)
) {
    return jwt_create_token({
        {"username", user.username},
        {"password", user.password}
    });
}

HTTP_ROUTE(
    ("/user/signup", ("POST")),
    (user_signup),
        (sql_db  , db  , http::arg::depends(db_dependency))
        (UserForm, user, http::arg::json                  ),
    (http::Result<std::string>)
) {
    if (user.username == "") {
        return Err(http::Error{http::StatusBadRequest, "Username cannot be empty"});
    } else if (user.password == "") {
        return Err(http::Error{http::StatusBadRequest, "Password cannot be empty"});
    }

    try {
        db(insert_into(users).set(
            users.username   = user.username,
            users.password   = password_hash(user.password),
            users.created_at = sqlpp::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
        ));
    } catch (const std::exception&) {
        return Err(http::Error{http::StatusConflict, "Username already exists"});
    }

    return Ok(user_create_token(user));
}

HTTP_ROUTE(
    ("/user/login", ("POST")),
    (user_login),
        (sql_db  , db  , http::arg::depends(db_dependency))
        (UserForm, user, http::arg::json                  ),
    (http::Result<std::string>)
) {
    bool found = false;
    bool valid_password = false;

    for (const auto& row : db(select(users.username, users.password).from(users).unconditionally())) {
        if ((found = row.username == user.username)) {
            valid_password = row.password == password_hash(user.password);
            break;
        }
    }

    if (not valid_password) {
        return Err(http::Error{http::StatusBadRequest, "Invalid password"});
    } else if (found) {
        return Ok(user_create_token(user));
    } else {
        return Err(http::Error{http::StatusBadRequest, "Username not found in the database"});
    }
}

HTTP_ROUTE(
    ("/user/verify", ("GET")),
    (user_verify),
        (const http::RequestReader&, req, http::arg::request )
        (http::ResponseWriter&     ,    , http::arg::response),
    (http::Result<std::string>)
) {
    auto it = req.headers.find("Authentication");
    if (it == req.headers.end()) {
        it = req.headers.find("authentication");
    }
    if (it == req.headers.end()) {
        return Err(http::Error{http::StatusUnauthorized, "Authentication is needed"});
    }

    std::string_view bearer = "Bearer ";
    if (not it->second.starts_with(bearer)) {
        return Err(http::Error{http::StatusUnauthorized, "Bearer authentication is needed"});
    }

    auto token = (std::string)it->second.substr(bearer.size());
    auto payload = TRY_OR(jwt_decode_token(token), {
        return Err(http::Error{http::StatusUnauthorized, try_res.unwrap_err().what});
    });

    auto user = TRY_OR(delameta::json::deserialize<UserForm>(payload), {
        return Err(http::Error{http::StatusInternalServerError, "Fail to deserialize JWT payload into UserForm"});
    });

    return Ok(user.username);
}

HTTP_ROUTE(
    ("/user/id", ("GET")),
    (user_get_id), 
        (const http::RequestReader&, req, http::arg::request )
        (http::ResponseWriter&     , res, http::arg::response),
    (http::Result<uint64_t>)
) {
    auto username = TRY(user_verify(req, res));
    auto db = db_dependency(req, res);

    auto row = db(select(users.id).from(users).where(users.username == username));
    if (row.begin() == row.end()) {
        return Err(http::Error{http::StatusBadRequest, "User not found in the database"});
    }

    auto& item = *row.begin();
    return Ok(item.id.value());
}

HTTP_ROUTE(
    ("/user", ("GET")),
    (user_get),
        (sql_db     , db      , http::arg::depends(db_dependency))
        (std::string, username, http::arg::depends(user_verify)  ),
    (http::Result<User>)
) {
    auto row = db(select(users.username, users.created_at).from(users).where(users.username == username));
    if (row.begin() == row.end()) {
        return Err(http::Error{http::StatusBadRequest, "User not found in the database"});
    }

    auto& item = *row.begin();
    return Ok(User{
        .username   = item.username.value(),
        .created_at = item.created_at.value(),
    });
}

HTTP_ROUTE(
    ("/users", ("GET")),
    (users_get),
        (std::string              ,         , http::arg::depends(user_verify)                 )
        (sql_db                   , db      , http::arg::depends(db_dependency)               )
        (std::optional<time_point>, date_min, http::arg::default_val("date-min", std::nullopt))
        (std::optional<time_point>, date_max, http::arg::default_val("date-max", std::nullopt))
        (unsigned int             , limit   , http::arg::default_val("limit", 10)             )
    ,
    (std::list<User>)
) {
    if (not date_min) date_min.emplace(time_point{});
    if (not date_max) date_max.emplace(std::chrono::system_clock::now());

    std::list<User> res;
    for (const auto& row : db(
        select(users.username, users.created_at)
            .from(users)
            .where(users.created_at >= *date_min and users.created_at <= *date_max)
            .order_by(users.created_at.desc())
            .limit(limit)
    )) {
        res.emplace_back(
            row.username.value(),
            row.created_at.value()
        );
    }

    return res;
}

HTTP_ROUTE(
    ("/user", ("DELETE")),
    (user_delete),
        (uint64_t, user_id, http::arg::depends(user_get_id)  )
        (sql_db  , db     , http::arg::depends(db_dependency)),
    (void)
) {
    db(remove_from(users).where(users.id == user_id));
    todos_delete(user_id, std::move(db));
}

TEST_CASE("1. user", "[user]") {
    SECTION("create table") {
        users_create_table("test.db");
    }

    static std::string token;
    SECTION("signup and login") {
        auto token_signup = user_signup(db_open("test.db"), {.username="Prapto", .password="qwerty"}).unwrap();
        auto token_login = user_login(db_open("test.db"), {.username="Prapto", .password="qwerty"}).unwrap();
        REQUIRE(token_signup == token_login);
        token = token_signup;
    };

    http::RequestReader req;
    http::ResponseWriter res;
    auto bearer_token = "Bearer " + token;
    req.headers["authentication"] = bearer_token;
    req.url.queries["db-path"] = "test.db";

    SECTION("verify") {
        auto username = user_verify(req, res).unwrap();
        REQUIRE(username == "Prapto");
    };

    SECTION("get id") {
        auto id = user_get_id(req, res).unwrap();
        REQUIRE(id == 1);
    };

    SECTION("invalid user") {
        auto err = user_get(db_open("test.db"), "Sugeng").unwrap_err();
        REQUIRE(err.what == "User not found in the database");
    };

    SECTION("list") {
        auto user_list = users_get("", db_open("test.db"), std::nullopt, std::nullopt, 10);
        REQUIRE(user_list.size() == 1);
        REQUIRE(user_list.front().username == "Prapto");
    };
}