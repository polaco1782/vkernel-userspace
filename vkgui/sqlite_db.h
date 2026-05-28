#ifndef VKGUI_SQLITE_DB_H
#define VKGUI_SQLITE_DB_H

#include "vkgui_common.h"

#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace vkgui {

class SQLiteStatement;

class SQLiteDatabase {
public:
    SQLiteDatabase() = default;
    SQLiteDatabase(const SQLiteDatabase&) = delete;
    auto operator=(const SQLiteDatabase&) -> SQLiteDatabase& = delete;
    SQLiteDatabase(SQLiteDatabase&& other) noexcept;
    auto operator=(SQLiteDatabase&& other) noexcept -> SQLiteDatabase&;
    ~SQLiteDatabase();

    [[nodiscard]] auto open(vk::string_view path) -> bool;
    void close();

    [[nodiscard]] auto exec(vk::string_view sql) -> bool;
    [[nodiscard]] auto prepare(vk::string_view sql, SQLiteStatement& statement) -> bool;

    [[nodiscard]] auto is_open() const -> bool { return handle_ != nullptr; }
    [[nodiscard]] auto last_error() const -> const std::string& { return last_error_; }
    [[nodiscard]] auto handle() const -> sqlite3* { return handle_; }

private:
    friend class SQLiteStatement;

    void set_error_from_sqlite(int result_code);
    void set_error_text(const char* message);

    sqlite3* handle_ = nullptr;
    std::string last_error_;
};

class SQLiteStatement {
public:
    enum class StepResult {
        row,
        done,
        error,
    };

    SQLiteStatement() = default;
    SQLiteStatement(const SQLiteStatement&) = delete;
    auto operator=(const SQLiteStatement&) -> SQLiteStatement& = delete;
    SQLiteStatement(SQLiteStatement&& other) noexcept;
    auto operator=(SQLiteStatement&& other) noexcept -> SQLiteStatement&;
    ~SQLiteStatement();

    [[nodiscard]] auto prepare(SQLiteDatabase& database, vk::string_view sql) -> bool;
    void finalize();

    [[nodiscard]] auto bind_int(int index, int value) -> bool;
    [[nodiscard]] auto bind_bool(int index, bool value) -> bool;
    [[nodiscard]] auto bind_double(int index, double value) -> bool;
    [[nodiscard]] auto bind_text(int index, vk::string_view value) -> bool;

    [[nodiscard]] auto step() -> StepResult;
    [[nodiscard]] auto reset() -> bool;
    void clear_bindings();

    [[nodiscard]] auto column_int(int index) const -> int;
    [[nodiscard]] auto column_double(int index) const -> double;
    [[nodiscard]] auto column_text(int index) const -> std::string;

    [[nodiscard]] auto valid() const -> bool { return handle_ != nullptr; }
    [[nodiscard]] auto last_error() const -> const std::string& { return last_error_; }

private:
    void set_error(int result_code, const char* fallback_message = nullptr);

    sqlite3_stmt* handle_ = nullptr;
    sqlite3* database_handle_ = nullptr;
    std::string last_error_;
};

} // namespace vkgui

#endif // VKGUI_SQLITE_DB_H
