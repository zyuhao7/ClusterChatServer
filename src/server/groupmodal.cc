#include "groupmoodal.hpp"
#include "db.h"

bool GroupModal::CreateGroup(Group& group)
{
    // 1. 组装 sql
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')", \
    group.GetName().c_str(), group.GetDesc().c_str());

    // 2. 执行sql语句
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.SetId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

void GroupModal::AddGroup(int userid, int groupid, string role)
{
    // 1. 组装 sql
    char sql[1024] = {0};
    sprintf(sql, "insert into groupuser values(%d, %d, '%s')", \
    groupid, userid, role.c_str());

    // 2. 执行sql语句
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.SetId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}