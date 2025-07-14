#include "usermodal.hpp"
#include "db.h"
#include <iostream>
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