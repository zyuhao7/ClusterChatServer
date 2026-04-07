#include "usermodal.hpp"
#include "db.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
using namespace std;

bool UserModal::Insert(User& user)
{
    // 1. 组装 sql 语句
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name, password, state) values('%s', '%s', '%s')", 
    user.GetName().c_str(),  user.GetPwd().c_str(), user.GetState().c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            // 获取插入成功的用户生成的主键 id
            user.SetId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

User UserModal::query(int id)
{
      // 1. 组装 sql 语句
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d", id);

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
           MYSQL_ROW row =  mysql_fetch_row(res);
           if(row != nullptr)
           {
                User user;
                user.SetId(atoi(row[0]));
                user.SetName(row[1]);
                user.SetPwd(row[2]);
                user.SetState(row[3]);
                mysql_free_result(res);
                return user;
           }
        }
    }
    return  User();
}

User UserModal::queryByName(const string &name)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from user where name = '%s' limit 1", name.c_str());

    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                User user;
                user.SetId(atoi(row[0]));
                user.SetName(row[1]);
                user.SetPwd(row[2]);
                user.SetState(row[3]);
                mysql_free_result(res);
                return user;
            }
            mysql_free_result(res);
        }
    }
    return User();
}

bool UserModal::updateState(User& user)
{
     // 1. 组装 sql 语句
    char sql[1024] = {0};
    
    sprintf(sql, "update user set state = '%s' where id = %d", user.GetState().c_str(), user.GetId());
    MySQL mysql;
    if(mysql.connect())
    {
       if(mysql.update(sql))
            return true;
    }
    return  false;
}

bool UserModal::updatePassword(int id, const string &hashed_password)
{
    char sql[2048] = {0};
    sprintf(sql, "update user set password = '%s' where id = %d", hashed_password.c_str(), id);
    MySQL mysql;
    if(mysql.connect())
    {
        return mysql.update(sql);
    }
    return false;
}

void UserModal::resetState()
{
      char sql[1024] = "update user set state = 'offline' where state = 'online'"; 
    
    MySQL mysql;
    if(mysql.connect())
    {
       mysql.update(sql);
    }
}
