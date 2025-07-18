#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.hpp"

class GroupUser : public User
{
 public:
    void SetRole(string role){this->role = role;}
    string GetRole(){return role;}

 private:
    string role;
};
#endif