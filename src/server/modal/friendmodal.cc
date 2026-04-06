#include "friendmodal.hpp"
#include "db.h"
#include <cstdio>
#include <cstdlib>

bool FriendModal::insert(int userid, int friendid)
{
     char sql[1024] = {0};
    sprintf(sql, "insert into friend(userid, friendid) values(%d, %d),(%d, %d)",
            userid, friendid, friendid, userid);
    MySQL mysql;
    if(mysql.connect())
    {
        return mysql.update(sql);
    }
    return false;
}

bool FriendModal::isFriend(int userid, int friendid)
{
    char sql[1024] = {0};
    sprintf(sql, "select 1 from friend where userid = %d and friendid = %d limit 1",
            userid, friendid);

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            bool is_friend = (row != nullptr);
            mysql_free_result(res);
            return is_friend;
        }
    }
    return false;
}

vector<User> FriendModal::query(int userid)
{
 char sql[1024] = {0};
    sprintf(sql, "select a.id, a.name, a.state from user a inner join friend b on b.friendid  = a.id \
    where b.userid = %d", userid);
    vector<User> vec;
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res))
            {
                User user;
                user.SetId(atoi(row[0]));
                user.SetName(row[1]);
                user.SetState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res); 
        }
    }
    return vec;
}
