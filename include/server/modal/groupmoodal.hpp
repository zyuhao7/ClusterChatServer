#ifndef GROUPMODAL_H
#define GROUPMODAL_H
#include "group.hpp"
#include <string>
#include <vector>
using namespace std;

class GroupModal
{  
 public:
    // 创建群组
    bool CreateGroup(Group& group);
    // 加入群组
    bool AddGroup(int userid, int groupid, string role);
    // 查询用户所在群组信息
    vector<Group> QueryUserGroupInfo(int userid);
    // 根据组id查询群其他用户,用于群发消息
    vector<int> QueryGroupUsers(int userid, int groupid);
    // 群组是否存在
    bool GroupExists(int groupid);
    // 用户是否已在群中
    bool IsUserInGroup(int userid, int groupid);
    // 查询用户在群里的角色，空字符串表示不在群里
    string QueryUserRole(int userid, int groupid);
    // 退群
    bool RemoveGroupUser(int userid, int groupid);
    // 更新群成员角色
    bool UpdateUserRole(int userid, int groupid, const string &role);

};

#endif
