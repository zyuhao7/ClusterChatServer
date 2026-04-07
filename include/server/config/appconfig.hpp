#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <cstdint>
#include <string>
#include <unordered_map>

struct DbSettings
{
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "root";
    std::string password = "123456";
    std::string database = "chat";
    std::string charset = "utf8mb4";
};

struct RedisSettings
{
    std::string host = "127.0.0.1";
    int port = 6379;
};

struct ServerSettings
{
    std::string host = "127.0.0.1";
    uint16_t port = 9120;
    int thread_num = 4;
    std::string log_level = "INFO";
    int bcrypt_cost = 10;
};

class AppConfig
{
public:
    static AppConfig &instance();

    bool loadFromFile(const std::string &path);
    const std::string &configPath() const;

    const DbSettings &db() const;
    const RedisSettings &redis() const;
    const ServerSettings &server() const;

    std::string getRaw(const std::string &key, const std::string &default_value = "") const;

private:
    AppConfig() = default;

    void applyRawMap();
    static std::string trim(const std::string &s);
    static bool parseInt(const std::string &text, int *out);

    std::string _config_path;
    std::unordered_map<std::string, std::string> _raw;
    DbSettings _db;
    RedisSettings _redis;
    ServerSettings _server;
};

#endif
