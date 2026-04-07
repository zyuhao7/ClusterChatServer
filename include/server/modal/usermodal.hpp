#ifndef USERMODAL_H
#define USERMODAL_H
#include "user.hpp"
// 操作数据库表
class UserModal
{
public:
    bool Insert(User& user);
    User query(int id);
    User queryByName(const string &name);
    bool updateState(User& user);
    bool updatePassword(int id, const string &hashed_password);
    void resetState();
};


#endif
