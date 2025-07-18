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
    void AddGroup(int userid, int groupid, string role);
    // 查询用户所在群组信息
    vector<Group> QueryUserGroupInfo(int userid);
    // 根据组id查询群其他用户,用于群发消息
    vector<int> QueryGroupUsers(int userid, int groupid);

};

#endif