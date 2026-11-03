#include "db_fixture.hpp"

#include "appconfig.hpp"

#include <mysql/mysql.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
const char *const kTables[] = {
    "user",
    "friend",
    "user_blacklist",
    "allgroup",
    "groupuser",
    "offlinemessage",
    "message_history",
    "admin_audit_log",
    "admin_operation_log"};

const char *const kAutoIncrementTables[] = {
    "user",
    "allgroup",
    "offlinemessage",
    "message_history",
    "admin_audit_log",
    "admin_operation_log"};

std::string trim(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
    {
        ++start;
    }
    if (start == value.size())
    {
        return "";
    }

    size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])))
    {
        --end;
    }
    return value.substr(start, end - start + 1);
}

std::string toLower(const std::string &value)
{
    std::string result = value;
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    return result;
}

bool startsWith(const std::string &value, const std::string &prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool fileExists(const std::string &path)
{
    std::ifstream in(path.c_str());
    return in.is_open();
}

std::string currentDirectory()
{
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == nullptr)
    {
        throw std::runtime_error("deterministic db fixture: getcwd failed");
    }
    return std::string(buffer);
}

std::string findRepoRoot()
{
#ifdef CLUSTER_CHAT_SOURCE_DIR
    const std::string configured = CLUSTER_CHAT_SOURCE_DIR;
    if (fileExists(configured + "/chat.sql"))
    {
        return configured;
    }
#endif

    std::string dir = currentDirectory();
    while (!dir.empty())
    {
        if (fileExists(dir + "/chat.sql"))
        {
            return dir;
        }
        const size_t slash = dir.find_last_of('/');
        if (slash == std::string::npos || slash == 0)
        {
            break;
        }
        dir = dir.substr(0, slash);
    }
    throw std::runtime_error("deterministic db fixture: cannot locate chat.sql");
}

std::string envOr(const char *name, const std::string &fallback)
{
    const char *value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        return fallback;
    }
    return std::string(value);
}

MYSQL *connectTo(const std::string &host, unsigned int port, const std::string &user,
                 const std::string &password, const std::string &schema)
{
    MYSQL *conn = mysql_init(nullptr);
    if (conn == nullptr)
    {
        throw std::runtime_error("deterministic db fixture: mysql_init failed");
    }

    const char *schema_name = schema.empty() ? nullptr : schema.c_str();
    if (mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), schema_name, port, nullptr, 0) == nullptr)
    {
        const std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("deterministic db fixture: mysql connect failed: " + error);
    }
    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}

void execOrThrow(MYSQL *conn, const std::string &sql, const std::string &context)
{
    if (mysql_query(conn, sql.c_str()) != 0)
    {
        throw std::runtime_error("deterministic db fixture: " + context + ": " + mysql_error(conn));
    }
}

std::vector<std::string> splitStatements(const std::string &script)
{
    std::vector<std::string> statements;
    std::string current;
    bool in_quote = false;
    for (size_t i = 0; i < script.size(); ++i)
    {
        const char c = script[i];
        if (c == '\'')
        {
            if (in_quote && i + 1 < script.size() && script[i + 1] == '\'')
            {
                current += c;
                current += script[++i];
                continue;
            }
            in_quote = !in_quote;
        }

        if (c == ';' && !in_quote)
        {
            const std::string statement = trim(current);
            if (!statement.empty())
            {
                statements.push_back(statement);
            }
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    const std::string trailing = trim(current);
    if (!trailing.empty())
    {
        statements.push_back(trailing);
    }
    return statements;
}

void loadSchema(MYSQL *conn, const std::string &schema_path)
{
    std::ifstream in(schema_path.c_str());
    if (!in.is_open())
    {
        throw std::runtime_error("deterministic db fixture: cannot read " + schema_path);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::vector<std::string> statements = splitStatements(buffer.str());
    for (size_t i = 0; i < statements.size(); ++i)
    {
        const std::string lower = toLower(statements[i]);
        if (startsWith(lower, "create database") || startsWith(lower, "use "))
        {
            continue;
        }
        execOrThrow(conn, statements[i], "apply schema");
    }
}

bool isKnownTable(const std::string &table_name)
{
    for (size_t i = 0; i < sizeof(kTables) / sizeof(kTables[0]); ++i)
    {
        if (table_name == kTables[i])
        {
            return true;
        }
    }
    return false;
}
} // namespace

DeterministicDbFixture::DeterministicDbFixture()
    : _ready(false)
{
}

DeterministicDbFixture::~DeterministicDbFixture()
{
    teardownSuite();
}

void DeterministicDbFixture::teardownSuite()
{
    if (!_ready)
    {
        return;
    }

    try
    {
        MYSQL *conn = connectTo(_cfg.host, _cfg.port, _cfg.user, _cfg.password, "");
        execOrThrow(conn, "DROP DATABASE IF EXISTS `" + _cfg.schema + "`", "drop isolated schema");
        mysql_close(conn);
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "deterministic db fixture cleanup failed: %s\n", error.what());
    }

    std::remove(_cfg.runtime_config_path.c_str());
    _ready = false;
}

void DeterministicDbFixture::bootstrapSuite()
{
    if (_ready)
    {
        resetTables();
        return;
    }

    const std::string root = findRepoRoot();
    const bool loaded = AppConfig::instance().loadFromFile(root + "/config/server.conf");
    const DbSettings &db = AppConfig::instance().db();

    std::ostringstream default_port;
    default_port << db.port;

    _cfg.host = envOr("MYSQL_HOST", loaded ? db.host : "127.0.0.1");
    const int port = std::atoi(envOr("MYSQL_PORT", default_port.str()).c_str());
    _cfg.port = (port > 0 && port <= 65535) ? static_cast<unsigned int>(port) : 3306;
    _cfg.user = envOr("MYSQL_USER", loaded ? db.user : "root");
    _cfg.password = envOr("MYSQL_PASSWORD", loaded ? db.password : "123456");
    _cfg.charset = "utf8mb4";
    _cfg.schema_template_path = root + "/chat.sql";

    std::ostringstream schema_name;
    schema_name << "chat_unit_test_" << getpid();
    _cfg.schema = schema_name.str();

    std::string runtime_template = "/tmp/cluster_chat_unit_runtime_XXXXXX";
    std::vector<char> runtime_path(runtime_template.begin(), runtime_template.end());
    runtime_path.push_back('\0');
    const int runtime_fd = mkstemp(&runtime_path[0]);
    if (runtime_fd < 0)
    {
        throw std::runtime_error("deterministic db fixture: cannot create runtime config");
    }
    close(runtime_fd);
    _cfg.runtime_config_path.assign(&runtime_path[0]);

    MYSQL *conn = connectTo(_cfg.host, _cfg.port, _cfg.user, _cfg.password, "");
    try
    {
        execOrThrow(conn, "DROP DATABASE IF EXISTS `" + _cfg.schema + "`", "drop isolated schema");
        execOrThrow(conn, "CREATE DATABASE `" + _cfg.schema + "` CHARACTER SET utf8mb4", "create isolated schema");
        execOrThrow(conn, "USE `" + _cfg.schema + "`", "select isolated schema");
        loadSchema(conn, _cfg.schema_template_path);
    }
    catch (...)
    {
        mysql_close(conn);
        throw;
    }
    mysql_close(conn);

    std::ofstream out(_cfg.runtime_config_path.c_str());
    if (!out.is_open())
    {
        throw std::runtime_error("deterministic db fixture: cannot write " + _cfg.runtime_config_path);
    }
    out << "# generated by tests/cpp/db_fixture.cc\n";
    out << "db.host = " << _cfg.host << "\n";
    out << "db.port = " << _cfg.port << "\n";
    out << "db.user = " << _cfg.user << "\n";
    out << "db.password = " << _cfg.password << "\n";
    out << "db.name = " << _cfg.schema << "\n";
    out << "db.charset = " << _cfg.charset << "\n";
    out.close();

    if (!AppConfig::instance().loadFromFile(_cfg.runtime_config_path))
    {
        throw std::runtime_error("deterministic db fixture: cannot load " + _cfg.runtime_config_path);
    }
    _ready = true;
}

void DeterministicDbFixture::resetTables()
{
    if (!_ready)
    {
        throw std::runtime_error("deterministic db fixture: bootstrapSuite() not called");
    }

    MYSQL *conn = connectTo(_cfg.host, _cfg.port, _cfg.user, _cfg.password, _cfg.schema);
    try
    {
        execOrThrow(conn, "SET FOREIGN_KEY_CHECKS = 0", "disable foreign key checks");
        for (size_t i = 0; i < sizeof(kTables) / sizeof(kTables[0]); ++i)
        {
            execOrThrow(conn, "DELETE FROM `" + std::string(kTables[i]) + "`", "clear table");
        }
        for (size_t i = 0; i < sizeof(kAutoIncrementTables) / sizeof(kAutoIncrementTables[0]); ++i)
        {
            execOrThrow(conn, "ALTER TABLE `" + std::string(kAutoIncrementTables[i]) + "` AUTO_INCREMENT = 1",
                        "reset auto increment");
        }
        execOrThrow(conn, "SET FOREIGN_KEY_CHECKS = 1", "enable foreign key checks");
    }
    catch (...)
    {
        mysql_query(conn, "SET FOREIGN_KEY_CHECKS = 1");
        mysql_close(conn);
        throw;
    }
    mysql_close(conn);
}

long long DeterministicDbFixture::rowCount(const std::string &table_name)
{
    if (!_ready)
    {
        throw std::runtime_error("deterministic db fixture: bootstrapSuite() not called");
    }
    if (!isKnownTable(table_name))
    {
        throw std::runtime_error("deterministic db fixture: unknown table " + table_name);
    }

    MYSQL *conn = connectTo(_cfg.host, _cfg.port, _cfg.user, _cfg.password, _cfg.schema);
    const std::string sql = "SELECT COUNT(*) FROM `" + table_name + "`";
    if (mysql_query(conn, sql.c_str()) != 0)
    {
        const std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("deterministic db fixture: row count failed: " + error);
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == nullptr)
    {
        const std::string error = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("deterministic db fixture: row count result failed: " + error);
    }

    long long count = -1;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row != nullptr && row[0] != nullptr)
    {
        count = std::atoll(row[0]);
    }
    mysql_free_result(result);
    mysql_close(conn);
    return count;
}
