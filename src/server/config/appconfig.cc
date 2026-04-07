#include "appconfig.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

AppConfig &AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

bool AppConfig::loadFromFile(const std::string &path)
{
    _config_path = path;
    _raw.clear();

    std::ifstream in(path.c_str());
    if (!in.is_open())
    {
        return false;
    }

    std::string line;
    while (std::getline(in, line))
    {
        std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#')
        {
            continue;
        }

        size_t pos = stripped.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }

        std::string key = trim(stripped.substr(0, pos));
        std::string value = trim(stripped.substr(pos + 1));
        if (!key.empty())
        {
            _raw[key] = value;
        }
    }

    applyRawMap();
    return true;
}

const std::string &AppConfig::configPath() const
{
    return _config_path;
}

const DbSettings &AppConfig::db() const
{
    return _db;
}

const RedisSettings &AppConfig::redis() const
{
    return _redis;
}

const ServerSettings &AppConfig::server() const
{
    return _server;
}

std::string AppConfig::getRaw(const std::string &key, const std::string &default_value) const
{
    auto it = _raw.find(key);
    if (it == _raw.end())
    {
        return default_value;
    }
    return it->second;
}

void AppConfig::applyRawMap()
{
    int num = 0;
    _db.host = getRaw("db.host", _db.host);
    if (parseInt(getRaw("db.port"), &num))
    {
        _db.port = num;
    }
    _db.user = getRaw("db.user", _db.user);
    _db.password = getRaw("db.password", _db.password);
    _db.database = getRaw("db.name", _db.database);
    _db.charset = getRaw("db.charset", _db.charset);

    _redis.host = getRaw("redis.host", _redis.host);
    if (parseInt(getRaw("redis.port"), &num))
    {
        _redis.port = num;
    }

    _server.host = getRaw("server.host", _server.host);
    if (parseInt(getRaw("server.port"), &num) && num > 0 && num <= 65535)
    {
        _server.port = static_cast<uint16_t>(num);
    }
    if (parseInt(getRaw("server.thread_num"), &num) && num > 0)
    {
        _server.thread_num = num;
    }
    _server.log_level = getRaw("server.log_level", _server.log_level);
    if (parseInt(getRaw("security.bcrypt_cost"), &num) && num >= 4 && num <= 16)
    {
        _server.bcrypt_cost = num;
    }
}

std::string AppConfig::trim(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
    {
        ++start;
    }
    if (start == s.size())
    {
        return "";
    }

    size_t end = s.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end])))
    {
        --end;
    }
    return s.substr(start, end - start + 1);
}

bool AppConfig::parseInt(const std::string &text, int *out)
{
    if (text.empty() || out == nullptr)
    {
        return false;
    }
    std::istringstream iss(text);
    int value = 0;
    iss >> value;
    if (iss.fail() || !iss.eof())
    {
        return false;
    }
    *out = value;
    return true;
}
