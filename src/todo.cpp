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
using time_point = std::chrono::system_clock::time_point;
using sql_db = sqlpp::sqlite3::connection;
namespace http = delameta::http;

extern auto db_open(const char*) -> sql_db;
extern auto db_dependency(const http::RequestReader&, http::ResponseWriter&) -> sql_db;
extern auto user_get_id(const http::RequestReader&, http::ResponseWriter&) -> http::Result<uint64_t>;

SQLPP_DECLARE_TABLE(
    (Todos)
    ,
    (id        , int         , SQLPP_PRIMARY_KEY)
    (user_id   , int         , SQLPP_NOT_NULL   )
    (task      , varchar(128), SQLPP_NOT_NULL   )
    (is_done   , bool        , SQLPP_NOT_NULL   )
    (created_at, timestamp   , SQLPP_NOT_NULL   )
)
static const Todos::Todos todos;

void todos_create_table(const char* path) {
    auto db = db_open(path);
    db.execute(R"(CREATE TABLE IF NOT EXISTS Todos (
        id         INTEGER        PRIMARY KEY,
        user_id    INTEGER        REFERENCES Users(id) ON DELETE CASCADE,
        task       VARCHAR(128)   NOT NULL,
        is_done    BOOL           NOT NULL,
        created_at TIMESTAMP      DEFAULT CURRENT_TIMESTAMP
    ))");
}

HTTP_SETUP(todos_setup, app) {
    todos_create_table(nullptr);
}

JSON_DECLARE(
    (Todo)
    ,
    (uint64_t   , id        )
    (std::string, task      )
    (bool       , is_done   )
    (time_point , created_at)
)

HTTP_ROUTE(
    ("/todo", ("POST")),
    (todo_create),
        (uint64_t        , user_id, http::arg::depends(user_get_id)                   )
        (sql_db          , db     , http::arg::depends(db_dependency)                 )
        (std::string_view, task   , http::arg::json_item("task")                      )
        (bool            , is_done, http::arg::json_item_default_val("is_done", false))
    ,
    (http::Result<uint64_t>)
) {
    if (task == "") {
        return Err(http::Error{http::StatusBadRequest, "Task cannot be empty"});
    }

    db(insert_into(todos).set(
        todos.user_id    = user_id,
        todos.task       = task,
        todos.is_done    = is_done,
        todos.created_at = sqlpp::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
    ));

    return Ok(db.last_insert_id());
}

HTTP_ROUTE(
    ("/todo", ("PUT")),
    (todo_put),
        (uint64_t                       , user_id, http::arg::depends(user_get_id)                          )
        (sql_db                         , db     , http::arg::depends(db_dependency)                        )
        (uint64_t                       , id     , http::arg::arg("id")                                     )
        (std::optional<std::string_view>, task   , http::arg::json_item_default_val("task", std::nullopt)   )
        (std::optional<bool>            , is_done, http::arg::json_item_default_val("is_done", std::nullopt))
    ,
    (http::Result<void>)
) {
    bool has_task = task.has_value();
    bool has_is_done = is_done.has_value();
    auto filter = todos.id == id && todos.user_id == user_id;

    if (has_task && has_is_done) {
        db(update(todos).set(todos.task = *task, todos.is_done = *is_done).where(filter));
    } else if (has_task) {
        db(update(todos).set(todos.task = *task).where(filter));
    } else if (has_is_done) {
        db(update(todos).set(todos.is_done = *is_done).where(filter));
    } else {
        return Err(http::Error{http::StatusBadRequest, "JSON field `task` and `is_done` are not specified"});
    }

    return Ok();
}

HTTP_ROUTE(
    ("/todo", ("DELETE")),
    (todo_delete),
        (uint64_t, user_id, http::arg::depends(user_get_id)  )
        (sql_db  , db     , http::arg::depends(db_dependency))
        (uint64_t, id     , http::arg::arg("id")             )
    ,
    (void)
) {
    db(remove_from(todos).where(todos.id == id && todos.user_id == user_id));
}

HTTP_ROUTE(
    ("/todos", ("GET")),
    (todos_get),
        (uint64_t                 , user_id , http::arg::depends(user_get_id)                 )
        (sql_db                   , db      , http::arg::depends(db_dependency)               )
        (std::optional<time_point>, date_min, http::arg::default_val("date-min", std::nullopt))
        (std::optional<time_point>, date_max, http::arg::default_val("date-max", std::nullopt))
        (unsigned int             , limit   , http::arg::default_val("limit", 10)             )
    ,
    (std::list<Todo>)
) {
    if (not date_min) date_min.emplace(time_point{});
    if (not date_max) date_max.emplace(std::chrono::system_clock::now());

    std::list<Todo> res;
    for (const auto& row : db(
        select(todos.id, todos.task, todos.is_done, todos.created_at)
            .from(todos)
            .where(todos.user_id == user_id and todos.created_at >= *date_min and todos.created_at <= *date_max)
            .order_by(todos.created_at.desc())
            .limit(limit)
    )) {
        res.emplace_back(
            row.id.value(),
            row.task.value(),
            row.is_done.value(),
            row.created_at.value()
        );
    }

    return res;
}

HTTP_ROUTE(
    ("/todos", ("DELETE")),
    (todos_delete),
        (uint64_t, user_id, http::arg::depends(user_get_id)  )
        (sql_db  , db     , http::arg::depends(db_dependency)),
    (void)
) {
    db(remove_from(todos).where(todos.user_id == user_id));
}

TEST_CASE("2. todo", "[todo]") {
    SECTION("create table") {
        todos_create_table("test.db");
    }

    auto get_todo_list = []() {
        return todos_get(1, db_open("test.db"), std::nullopt, std::nullopt, 10);
    };

    auto todo_compare = [](const Todo& self, const Todo& other) {
        REQUIRE(self.id == other.id);
        REQUIRE(self.task == other.task);
        REQUIRE(self.is_done == other.is_done);
    };

    SECTION("create") {
        auto first_id = todo_create(1, db_open("test.db"), "new task", false).unwrap();
        REQUIRE(first_id == 1);
    
        auto second_id = todo_create(1, db_open("test.db"), "second task", false).unwrap();
        REQUIRE(second_id == 2);

        auto todo_list = get_todo_list();
        REQUIRE(todo_list.size() == 2);

        todo_compare(todo_list.front(), Todo{.id=1, .task="new task", .is_done=false, .created_at={}});
        todo_compare(todo_list.back(), Todo{.id=2, .task="second task", .is_done=false, .created_at={}});
    }

    SECTION("update") {
        todo_put(1, db_open("test.db"), 1, "first task", std::nullopt).unwrap();
        todo_put(1, db_open("test.db"), 2, std::nullopt, true).unwrap();

        auto todo_list = get_todo_list();
        REQUIRE(todo_list.size() == 2);

        todo_compare(todo_list.front(), Todo{.id=1, .task="first task", .is_done=false, .created_at={}});
        todo_compare(todo_list.back(), Todo{.id=2, .task="second task", .is_done=true, .created_at={}});
    }

    SECTION("delete") {
        todo_delete(1, db_open("test.db"), 1);

        auto todo_list = get_todo_list();
        REQUIRE(todo_list.size() == 1);

        todo_compare(todo_list.front(), Todo{.id=2, .task="second task", .is_done=true, .created_at={}});
    }
}