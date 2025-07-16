#ifndef OFFLINEMSGMODAL_H
#define OFFLINEMSGMODAL_H   
#include <vector>
#include <string>

using namespace std;

// 离线消息模型
class OfflineMsgModal
{
public:
    // 插入离线消息
    void insert(int userid, string msg);
    // 删除用户的离线消息
    void remove(int userid);
    // 查询用户的离线消息
    vector<string> query(int userid);
};

#endif