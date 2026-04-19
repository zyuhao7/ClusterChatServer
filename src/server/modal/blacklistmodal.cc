#include "blacklistmodal.hpp"

#include "db.h"
#include <cstdio>

bool BlacklistModal::insert(int userid, int blocked_userid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "insert into user_blacklist(userid, blocked_userid) values(%d, %d)",
            userid,
            blocked_userid);

    MySQL mysql;
    if(mysql.connect())
    {
        return mysql.update(sql);
    }
    return false;
}

bool BlacklistModal::remove(int userid, int blocked_userid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "delete from user_blacklist where userid = %d and blocked_userid = %d",
            userid,
            blocked_userid);

    MySQL mysql;
    if(mysql.connect())
    {
        return mysql.update(sql);
    }
    return false;
}

bool BlacklistModal::isBlocked(int userid, int blocked_userid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "select 1 from user_blacklist where userid = %d and blocked_userid = %d limit 1",
            userid,
            blocked_userid);

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            bool blocked = (row != nullptr);
            mysql_free_result(res);
            return blocked;
        }
    }
    return false;
}
