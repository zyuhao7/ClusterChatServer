#include "db.h"
#include "appconfig.hpp"

bool MySQL::connect()
{
    const DbSettings &cfg = AppConfig::instance().db();
    MYSQL *p = mysql_real_connect(
        _conn,
        cfg.host.c_str(),
        cfg.user.c_str(),
        cfg.password.c_str(),
        cfg.database.c_str(),
        cfg.port,
        nullptr,
        0);
    if (p != nullptr)
    {
         string charset_sql = "set names " + cfg.charset;
         mysql_query(_conn, charset_sql.c_str());
         LOG_INFO << "connect mysql success!";
    }
    else{
        LOG_INFO << "connect mysql failed";
    }
    return p;
}

// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":"  << sql << "更新失败!";
        return false;
    }
    return true;
}

// 查询操作
MYSQL_RES* MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
        << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

 MYSQL* MySQL::getConnection()
 {
    return _conn;
 }
