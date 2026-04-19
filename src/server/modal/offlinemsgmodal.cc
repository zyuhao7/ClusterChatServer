#include "offlinemsgmodal.hpp"
#include "appconfig.hpp"
#include "db.h"
#include <cstdlib>
#include <cstdio>

namespace
{
int queryExcessOfflineMessages(MySQL &mysql, int userid, int limit)
{
    if (limit <= 0)
    {
        return 0;
    }

    char sql[256] = {0};
    sprintf(sql, "select count(*) from offlinemessage where userid = %d", userid);
    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return 0;
    }

    int excess = 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row != nullptr)
    {
        int total = atoi(row[0]);
        if (total > limit)
        {
            excess = total - limit;
        }
    }
    mysql_free_result(res);
    return excess;
}
}

void OfflineMsgModal::insert(int userid, string msg, const string &request_id)
{
    char sql[2048] = {0};
    sprintf(sql, "insert into offlinemessage(userid, message, request_id) values(%d, '%s', '%s')",
            userid, msg.c_str(), request_id.c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if (mysql.update(sql))
        {
            int excess = queryExcessOfflineMessages(
                mysql,
                userid,
                AppConfig::instance().server().offline_message_limit);
            if (excess > 0)
            {
                char trim_sql[256] = {0};
                sprintf(
                    trim_sql,
                    "delete from offlinemessage where userid = %d order by id asc limit %d",
                    userid,
                    excess);
                mysql.update(trim_sql);
            }
        }
    }
}

void OfflineMsgModal::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);   
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

vector<string> OfflineMsgModal::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d order by id asc", userid);
    vector<string> vec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res))
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res); 
        }
    }
    return vec;
}
