#ifndef USER_H
#define USER_H
#include <string>
using namespace std;
/// @file user.hpp
/// @brief 定义用户类，用于表示用户的基本信息

/// @class User
/// @brief 用户类，表示系统中的用户基本信息
///
/// 该类包含用户的ID、用户名、密码以及状态（如在线或离线）。
/// 提供了对用户信息的设置和获取方法。
///
/// 功能包括：
/// - 设置和获取用户的ID、用户名、密码和状态
/// - 默认状态为 "offline"
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }
    void SetId(int id) { this->id = id; }
    void SetName(string name) { this->name = name; }
    void SetPwd(string pwd) { this->password = pwd; }
    void SetState(string state) { this->state = state; }

    int GetId() { return id; }
    string GetName() { return name; }
    string GetPwd() { return password; }
    string GetState() { return state; }

protected:
    int id;
    string name;
    string password;
    string state;
};

#endif