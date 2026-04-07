#include "groupmoodal.hpp"
#include "groupuser.hpp"
#include "db.h"
#include <cstdio>
#include <cstdlib>

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

bool GroupModal::AddGroup(int userid, int groupid, string role)
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
         return true;
      }
   }
   return false;
}

 vector<Group> GroupModal::QueryUserGroupInfo(int userid)
 {
    // 1. 组装 sql
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from allgroup a \
    inner join groupuser b on a.id = b.groupid where b.userid = %d", userid);
    vector<Group> groupVec; 
    // 2. 执行sql语句
    MySQL mysql;
    if(mysql.connect())
    { 
        MYSQL_RES* res = mysql.query(sql);
        if(res)
        {
            //查出userid所有的群组信息
            while(MYSQL_ROW row = mysql_fetch_row(res))
            {
                Group group;
                group.SetId(atoi(row[0]));
                group.SetName(row[1]);
                group.SetDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    // 3. 从群组信息中提取用户id
    for(auto& group : groupVec)
    {
        sprintf(sql,"select a.id, a.name, a.state, b.grouprole from user a\
            inner join groupuser b on b.userid = a.id where b.groupid = %d", group.GetId());
        MYSQL_RES* res = mysql.query(sql);
        if(res)
        {
            while(MYSQL_ROW row = mysql_fetch_row(res))
            {
                GroupUser user;
                user.SetId(atoi(row[0]));
                user.SetName(row[1]);
                user.SetState(row[2]);
                user.SetRole(row[3]);
                group.GetUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
 }

 vector<int> GroupModal::QueryGroupUsers(int userid, int groupid)
 {
    vector<int> useridVec;
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res = mysql.query(sql);
        if(res)
        {
            while(MYSQL_ROW row = mysql_fetch_row(res))
            {
                useridVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return useridVec;
 }

bool GroupModal::GroupExists(int groupid)
{
    char sql[256] = {0};
    sprintf(sql, "select 1 from allgroup where id = %d limit 1", groupid);
    MySQL mysql;
    if (!mysql.connect())
    {
        return false;
    }
    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = (row != nullptr);
    mysql_free_result(res);
    return exists;
}

bool GroupModal::IsUserInGroup(int userid, int groupid)
{
    return !QueryUserRole(userid, groupid).empty();
}

string GroupModal::QueryUserRole(int userid, int groupid)
{
    char sql[256] = {0};
    sprintf(sql,
            "select grouprole from groupuser where groupid = %d and userid = %d limit 1",
            groupid, userid);
    MySQL mysql;
    if (!mysql.connect())
    {
        return "";
    }
    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return "";
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    string role = row != nullptr && row[0] != nullptr ? row[0] : "";
    mysql_free_result(res);
    return role;
}

bool GroupModal::RemoveGroupUser(int userid, int groupid)
{
    char sql[256] = {0};
    sprintf(sql, "delete from groupuser where groupid = %d and userid = %d", groupid, userid);
    MySQL mysql;
    if (!mysql.connect())
    {
        return false;
    }
    return mysql.update(sql);
}

bool GroupModal::UpdateUserRole(int userid, int groupid, const string &role)
{
    char sql[512] = {0};
    sprintf(sql,
            "update groupuser set grouprole = '%s' where groupid = %d and userid = %d",
            role.c_str(), groupid, userid);
    MySQL mysql;
    if (!mysql.connect())
    {
        return false;
    }
    return mysql.update(sql);
}
