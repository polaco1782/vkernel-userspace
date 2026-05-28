#include "sqlite_db.h"

#include <sqlite3.h>

namespace vkgui {

namespace {

auto string_for_sql(vk::string_view view) -> std::string
{
    return string_from_view(view);
}

} // namespace

SQLiteDatabase::SQLiteDatabase(SQLiteDatabase&& other) noexcept
    : handle_(other.handle_)
    , last_error_(std::move(other.last_error_))
{
    other.handle_ = nullptr;
}

auto SQLiteDatabase::operator=(SQLiteDatabase&& other) noexcept -> SQLiteDatabase&
{
    if (this == &other) {
        return *this;
    }

    close();
    handle_ = other.handle_;
    last_error_ = std::move(other.last_error_);
    other.handle_ = nullptr;
    return *this;
}

SQLiteDatabase::~SQLiteDatabase()
{
    close();
}

auto SQLiteDatabase::open(vk::string_view path) -> bool
{
    close();

    const std::string path_text = string_for_sql(path);
    sqlite3* database_handle = nullptr;
    const int result = sqlite3_open_v2(path_text.c_str(),
                                       &database_handle,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                       nullptr);
    if (result != SQLITE_OK) {
        handle_ = database_handle;
        set_error_from_sqlite(result);
        close();
        return false;
    }

    handle_ = database_handle;
    last_error_.clear();
    return true;
}

void SQLiteDatabase::close()
{
    if (handle_ == nullptr) {
        return;
    }

    (void)sqlite3_close(handle_);
    handle_ = nullptr;
}

auto SQLiteDatabase::exec(vk::string_view sql) -> bool
{
    if (handle_ == nullptr) {
        set_error_text("database is not open");
        return false;
    }

    const std::string sql_text = string_for_sql(sql);
    char* error_text = nullptr;
    const int result = sqlite3_exec(handle_, sql_text.c_str(), nullptr, nullptr, &error_text);
    if (result != SQLITE_OK) {
        if (error_text != nullptr) {
            last_error_ = error_text;
            sqlite3_free(error_text);
        } else {
            set_error_from_sqlite(result);
        }
        return false;
    }

    last_error_.clear();
    return true;
}

auto SQLiteDatabase::prepare(vk::string_view sql, SQLiteStatement& statement) -> bool
{
    return statement.prepare(*this, sql);
}

void SQLiteDatabase::set_error_from_sqlite(int result_code)
{
    if (handle_ != nullptr) {
        const char* message = sqlite3_errmsg(handle_);
        if (message != nullptr) {
            last_error_ = message;
            return;
        }
    }

    const char* message = sqlite3_errstr(result_code);
    last_error_ = message != nullptr ? message : "sqlite error";
}

void SQLiteDatabase::set_error_text(const char* message)
{
    last_error_ = message != nullptr ? message : "sqlite error";
}

SQLiteStatement::SQLiteStatement(SQLiteStatement&& other) noexcept
    : handle_(other.handle_)
    , database_handle_(other.database_handle_)
    , last_error_(std::move(other.last_error_))
{
    other.handle_ = nullptr;
    other.database_handle_ = nullptr;
}

auto SQLiteStatement::operator=(SQLiteStatement&& other) noexcept -> SQLiteStatement&
{
    if (this == &other) {
        return *this;
    }

    finalize();
    handle_ = other.handle_;
    database_handle_ = other.database_handle_;
    last_error_ = std::move(other.last_error_);
    other.handle_ = nullptr;
    other.database_handle_ = nullptr;
    return *this;
}

SQLiteStatement::~SQLiteStatement()
{
    finalize();
}

auto SQLiteStatement::prepare(SQLiteDatabase& database, vk::string_view sql) -> bool
{
    finalize();

    if (!database.is_open()) {
        last_error_ = "database is not open";
        return false;
    }

    const std::string sql_text = string_for_sql(sql);
    sqlite3_stmt* statement_handle = nullptr;
    const int result = sqlite3_prepare_v2(database.handle(),
                                          sql_text.c_str(),
                                          static_cast<int>(sql_text.size()),
                                          &statement_handle,
                                          nullptr);
    if (result != SQLITE_OK) {
        database_handle_ = database.handle();
        set_error(result);
        return false;
    }

    handle_ = statement_handle;
    database_handle_ = database.handle();
    last_error_.clear();
    return true;
}

void SQLiteStatement::finalize()
{
    if (handle_ != nullptr) {
        (void)sqlite3_finalize(handle_);
    }
    handle_ = nullptr;
    database_handle_ = nullptr;
    last_error_.clear();
}

auto SQLiteStatement::bind_int(int index, int value) -> bool
{
    const int result = sqlite3_bind_int(handle_, index, value);
    if (result != SQLITE_OK) {
        set_error(result);
        return false;
    }
    return true;
}

auto SQLiteStatement::bind_bool(int index, bool value) -> bool
{
    return bind_int(index, value ? 1 : 0);
}

auto SQLiteStatement::bind_double(int index, double value) -> bool
{
    const int result = sqlite3_bind_double(handle_, index, value);
    if (result != SQLITE_OK) {
        set_error(result);
        return false;
    }
    return true;
}

auto SQLiteStatement::bind_text(int index, vk::string_view value) -> bool
{
    const char* text = value.empty() ? "" : value.data();
    const int result = sqlite3_bind_text(handle_,
                                         index,
                                         text,
                                         static_cast<int>(value.size()),
                                         SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        set_error(result);
        return false;
    }
    return true;
}

auto SQLiteStatement::step() -> StepResult
{
    const int result = sqlite3_step(handle_);
    if (result == SQLITE_ROW) {
        return StepResult::row;
    }
    if (result == SQLITE_DONE) {
        return StepResult::done;
    }

    set_error(result);
    return StepResult::error;
}

auto SQLiteStatement::reset() -> bool
{
    const int result = sqlite3_reset(handle_);
    if (result != SQLITE_OK) {
        set_error(result);
        return false;
    }
    return true;
}

void SQLiteStatement::clear_bindings()
{
    if (handle_ != nullptr) {
        (void)sqlite3_clear_bindings(handle_);
    }
}

auto SQLiteStatement::column_int(int index) const -> int
{
    return sqlite3_column_int(handle_, index);
}

auto SQLiteStatement::column_double(int index) const -> double
{
    return sqlite3_column_double(handle_, index);
}

auto SQLiteStatement::column_text(int index) const -> std::string
{
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(handle_, index));
    if (text == nullptr) {
        return std::string();
    }

    const int length = sqlite3_column_bytes(handle_, index);
    return std::string(text, static_cast<size_t>(length));
}

void SQLiteStatement::set_error(int result_code, const char* fallback_message)
{
    if (database_handle_ != nullptr) {
        const char* message = sqlite3_errmsg(database_handle_);
        if (message != nullptr) {
            last_error_ = message;
            return;
        }
    }

    if (fallback_message != nullptr) {
        last_error_ = fallback_message;
        return;
    }

    const char* message = sqlite3_errstr(result_code);
    last_error_ = message != nullptr ? message : "sqlite error";
}

} // namespace vkgui
