#ifndef USERMODAL_H
#define USERMODAL_H
#include "user.hpp"
// 操作数据库表
class UserModal
{
public:
    bool Insert(User& user);
    User query(int id);
    bool updateState(User& user);
    void resetState();
};


#endif