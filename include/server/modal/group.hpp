#ifndef GROUP_H
#define GROUP_H
#include <iostream>
#include <string>
#include <vector>
#include "groupuser.hpp"
using namespace std;

/// @file group.hpp
/// @brief 定义群组类，用于表示群组的基本信息和成员管理

/// @class Group
/// @brief 群组类，包含群组的ID、名称、描述以及成员列表
///
/// 该类提供了对群组基本信息的设置和获取方法，同时维护了一个群组成员的列表。
/// 成员列表由 GroupUser 类型的对象组成。
///
/// 功能包括：
/// - 设置和获取群组的ID、名称、描述
/// - 获取群组成员列表

class Group
{
public:
    Group(int id = -1, string name = "", string desc = "", string announcement = "")
        : id_(id),
          name_(name),
          desc_(desc),
          announcement_(announcement)
    {
    }
    void SetId(int id) { id_ = id; }
    void SetName(string name) { name_ = name; }
    void SetDesc(string desc) { desc_ = desc; }
    void SetAnnouncement(string announcement) { announcement_ = announcement; }

    int GetId() { return id_; }
    string GetName() { return name_; }
    string GetDesc() { return desc_; }
    string GetAnnouncement() { return announcement_; }
    vector<GroupUser> &GetUsers() { return users; }

private:
    int id_;
    string name_;
    string desc_;
    string announcement_;
    vector<GroupUser> users;
};

#endif
