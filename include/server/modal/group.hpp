#ifndef GROUP_H
#define GROUP_H
#include <iostream>
#include <string>
#include <vector>
#include "groupuser.hpp"
using namespace std;

class Group
{
 public:
    Group(int id = -1, string name = "", string desc ="")
        :id_(id),
        name_(name),
        desc_(desc)
    {}
    void SetId(int id){id_ = id;}
    void SetName(string name){name_ = name;}
    void SetDesc(string desc){desc_ = desc;}

    int GetId() {return id_;}
    string GetName(){return name_;}
    string GetDesc(){return desc_;}
    vector<GroupUser>& GetUsers(){return users;}

 private:
    int id_;
    string name_;
    string desc_;
    vector<GroupUser> users;
};

#endif

