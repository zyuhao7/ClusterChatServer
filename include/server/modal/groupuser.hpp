#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.hpp"

/// @file groupuser.hpp
/// @brief 定义群组用户类，用于扩展用户在群组中的角色信息

/// @class GroupUser
/// @brief 群组用户类，继承自 User 类，表示用户在群组中的信息
///
/// 该类在 User 类的基础上增加了角色信息，用于描述用户在群组中的角色（如管理员、普通成员等）。
///
/// 功能包括：
/// - 设置和获取用户在群组中的角色
class GroupUser : public User
{
public:
   void SetRole(string role) { this->role = role; }
   string GetRole() { return role; }

private:
   string role;
};
#endif